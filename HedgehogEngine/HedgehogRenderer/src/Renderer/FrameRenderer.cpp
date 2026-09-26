#include "FrameRenderer.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogExtract/api/CameraMath.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        // Pass data and closures for one frame: the shared phase plus a few views.
        constexpr size_t GRAPH_ARENA_BYTES = 256 * 1024;

        constexpr const char* SHIPPED_GRAPHS[] = { "scene", "game", "result" };
        constexpr const char* GRAPH_DIRECTORY  = "engine://HedgehogEngine/HedgehogRenderer/assets/Graphs/";
    }

    FrameRenderer::FrameRenderer(RHI::IRHIDevice& device, RHI::IRHISwapchain& swapchain,
                                 const FS::FileSystemManager& fileSystem)
        : m_Device(device)
        , m_Swapchain(swapchain)
        , m_Services(device, fileSystem)
        , m_Library(m_Registry)
        , m_Instantiator(m_Registry)
        , m_Targets(device, swapchain)
        , m_Views(m_Targets)
        , m_Shared(m_Registry)
    {
        RegisterEnginePassTypes(m_Registry);
        for (const char* name : SHIPPED_GRAPHS)
        {
            const auto file = fileSystem.ResolvePhysical(std::string(GRAPH_DIRECTORY) + name + ".graph");
            if (!file || !m_Library.Register(name, *file))
                LOGERROR("FrameRenderer: the shipped graph '", name, "' could not be loaded.");
        }

        const RenderTargetResult viewport = m_Targets.Declare(
            { VIEWPORT_TARGET, RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeRelativeToSwapchain(1.0f) });
        if (!viewport.Success)
            LOGERROR("FrameRenderer: ", viewport.Message);

        for (auto& runtime : m_Runtimes)
            runtime = std::make_unique<RenderGraphRuntime>(device, GRAPH_ARENA_BYTES);
    }

    // The owner waits for the device to go idle first, as for every other GPU resource.
    FrameRenderer::~FrameRenderer() = default;

    void FrameRenderer::Render(const HX::RenderScene& scene, const HR::ResourceRegistry& resources,
                               const HedgehogSettings::Settings& settings, const UiCallback& ui,
                               const FrameSync& sync)
    {
        const uint32_t imageIndex = m_Swapchain.AcquireNextImage(sync.ImageAvailable);
        sync.Fence.Reset();
        sync.Cmd.Reset();
        sync.Cmd.Begin();

        // This slot's fence has signaled, so every frame up to MAX_FRAMES_IN_FLIGHT ago is complete.
        ++m_FrameNumber;
        const uint64_t completed = m_FrameNumber > HedgehogEngine::MAX_FRAMES_IN_FLIGHT
                                 ? m_FrameNumber - HedgehogEngine::MAX_FRAMES_IN_FLIGHT : 0;
        m_Targets.BeginFrame(m_FrameNumber, completed, imageIndex);
        m_Services.BeginFrame(sync.FrameIndex);
        m_Library.Poll();

        const std::vector<View>& views = m_Views.BuildViews(scene);
        FillSceneFrame(scene, resources, settings);

        // Filled completely before any pointer into them is taken.
        m_ViewFrames.clear();
        m_ViewInstances.resize(views.size());
        for (size_t i = 0; i < views.size(); ++i)
        {
            m_ViewFrames.push_back(MakeViewFrame(views[i], m_ViewInstances[i]));
            m_ViewFrames.back().Ui = ui;
        }
        m_ViewContexts.assign(views.size(), GraphFrameContext{});
        m_ViewSampledTargets.resize(views.size());
        m_WrittenTargets.clear();

        // The shadows are fitted to one view's camera, but cast by every scene instance: an object
        // the view culls or hides still shadows what it shows.
        const View*           shadowView  = SelectShadowView(views);
        const GraphFrameData* shadowFrame = nullptr;
        if (shadowView)
        {
            m_ShadowViewFrame                  = m_ViewFrames[static_cast<size_t>(shadowView - views.data())];
            m_ShadowViewFrame.OpaqueInstances  = m_SceneFrame.OpaqueInstances;
            m_ShadowViewFrame.OverlayInstances = {};
            shadowFrame                        = &m_ShadowViewFrame;
        }

        const auto& shadowmap = *settings.GetShadowmapSettings();
        SharedPhaseSettings sharedSettings;
        sharedSettings.ShadowAtlasSize  = shadowmap.GetShadowmapSize();
        sharedSettings.ShadowCasterMask = shadowmap.GetShadowCasterMask();

        RenderGraphRuntime&      graph  = *m_Runtimes[sync.FrameIndex];
        const SharedPhaseOutputs shared =
            m_Shared.Declare(graph, m_Services, shadowFrame, scene.Lights, sharedSettings);

        // Only one view presents: the highest priority, the earliest on a tie.
        const View* presenter = nullptr;
        for (const View& view : views)
        {
            if (Presents(view) && (!presenter || view.Desc.Priority > presenter->Desc.Priority))
                presenter = &view;
        }

        PresentSource presented = PresentSource::None;
        for (size_t i = 0; i < views.size(); ++i)
        {
            if (Presents(views[i]) && &views[i] != presenter)
            {
                ReportOnce("FrameRenderer: view " + std::to_string(views[i].Id) + " also targets the presented "
                           "surface; view " + std::to_string(presenter->Id) + " has a higher priority and "
                           "presents (RENDERING.md section 8).");
                continue;
            }
            const std::optional<PresentSource> source =
                DeclareView(graph, views[i], m_ViewFrames[i], m_ViewContexts[i], shared);
            if (source && &views[i] == presenter)
                presented = *source;
        }

        if (!graph.Execute(sync.Cmd))
            presented = PresentSource::None;
        m_LastFramePassCount = graph.GetLastExecutedPassCount();

        Present(sync, imageIndex, presented);

        m_Targets.EndFrame();
        m_Views.EndFrame();
    }

    void FrameRenderer::FillSceneFrame(const HX::RenderScene& scene, const HR::ResourceRegistry& resources,
                                       const HedgehogSettings::Settings& settings)
    {
        m_Meshes.clear();
        for (size_t i = 0; i < resources.GetMeshCount(); ++i)
        {
            const HR::MeshGeometryInfo& mesh = resources.GetMeshGeometryInfo(i);
            m_Meshes.push_back({ mesh.FirstIndex, mesh.IndexCount, mesh.VertexOffset });
        }
        m_MaterialSets.clear();
        for (size_t i = 0; i < resources.GetMaterialCount(); ++i)
            m_MaterialSets.push_back(&resources.GetMaterialDescriptorSet(static_cast<uint32_t>(i)));

        GraphFrameData frame;
        // Every visible instance is drawn as opaque: RenderScene carries no material type yet.
        m_FrameInstances = scene.Instances;
        CollectSceneInstances(scene.Instances, m_SceneInstances);
        frame.OpaqueInstances = m_SceneInstances;
        frame.Meshes          = m_Meshes;
        frame.MaterialSets    = m_MaterialSets;
        if (!m_Meshes.empty())
        {
            // The passes bind these but never write them; the command list API takes them non-const.
            frame.Positions = const_cast<RHI::IRHIBuffer*>(&resources.GetPositionsBuffer());
            frame.TexCoords = const_cast<RHI::IRHIBuffer*>(&resources.GetTexCoordsBuffer());
            frame.Normals   = const_cast<RHI::IRHIBuffer*>(&resources.GetNormalsBuffer());
            frame.Indices   = const_cast<RHI::IRHIBuffer*>(&resources.GetIndexBuffer());
        }
        else
        {
            frame.OpaqueInstances = {}; // nothing to draw them from
        }

        for (const HX::RenderLight& light : scene.Lights)
        {
            if (light.CastShadows)
            {
                frame.ShadowLightDirection = light.Direction;
                break;
            }
        }
        const auto& shadowmap          = *settings.GetShadowmapSettings();
        frame.ShadowCascadeCount       = shadowmap.GetCascadesCount();
        frame.ShadowCascadeSplitLambda = shadowmap.GetCascadeSplitLambda();
        m_SceneFrame = frame;
    }

    GraphFrameData FrameRenderer::MakeViewFrame(const View& view, ViewInstances& instances) const
    {
        GraphFrameData frame = m_SceneFrame;
        if (!view.Desc.Camera || view.ResolvedTargets.empty())
            return frame;

        const HX::RenderCamera& camera = *view.Desc.Camera;
        const TargetExtent&     extent = view.ResolvedTargets[0].Extent;
        const float             aspect = static_cast<float>(extent.Width) / static_cast<float>(extent.Height);

        frame.View        = HX::MakeViewMatrix(camera);
        frame.Proj        = HX::MakeProjection(camera, aspect);
        if (!m_Meshes.empty())
        {
            // Every instance, the editor layer included: the view's mask decides what it sees.
            CullViewInstances(m_FrameInstances, view.Desc.LayerMask, frame.Proj * frame.View, instances);
            frame.OpaqueInstances  = instances.Opaque;
            frame.OverlayInstances = instances.Overlay;
        }
        const HM::Vector4& position = camera.WorldMatrix[3]; // the translation column
        frame.EyePosition = HM::Vector3(position.x(), position.y(), position.z());
        frame.NearPlane   = camera.NearPlane;
        frame.FarPlane    = camera.FarPlane;
        return frame;
    }

    std::optional<FrameRenderer::PresentSource> FrameRenderer::DeclareView(
        RenderGraphRuntime& graph, const View& view, GraphFrameData& frame, GraphFrameContext& context,
        const SharedPhaseOutputs& shared)
    {
        const GraphAsset* asset = m_Library.Find(view.Desc.GraphName);
        if (!asset)
        {
            ReportOnce("FrameRenderer: view " + std::to_string(view.Id) + " uses the unknown graph '"
                       + view.Desc.GraphName + "'; it is skipped.");
            return std::nullopt;
        }

        // The targets this view reads, as the views before it wrote them this frame. One not
        // written this frame (its view hidden or dropped) is not sampled.
        std::vector<RGTexture>& sampled = m_ViewSampledTargets[static_cast<size_t>(&frame - m_ViewFrames.data())];
        sampled.clear();
        for (const std::string& read : view.Desc.Reads)
        {
            if (const auto written = m_WrittenTargets.find(read); written != m_WrittenTargets.end())
                sampled.push_back(written->second);
        }
        frame.UiSampledTargets = sampled;
        frame.SceneLights      = shared.SceneLights;
        context           = { &m_Services, &frame };
        graph.SetFrameContext(&context);

        const TargetExtent resultExtent = view.ResolvedTargets[0].Extent;
        const TargetExtent swapchain    = m_Targets.Resolve(RenderTargetRegistry::MAIN_TARGET).Extent;
        graph.SetSizeReferences(resultExtent.Width, resultExtent.Height, swapchain.Width, swapchain.Height);

        // The view's targets, imported so its graph writes them in place.
        PresentSource source = PresentSource::None;
        m_OutputTargets.clear();
        m_OutputContract.clear();
        for (size_t i = 0; i < view.ResolvedTargets.size(); ++i)
        {
            // A graph that cannot write the swapchain's format presents main through the viewport.
            const bool isMain      = view.Desc.Targets[i] == RenderTargetRegistry::MAIN_TARGET;
            const bool viaViewport = isMain && i < asset->Outputs.size()
                                  && asset->Outputs[i].Format != view.ResolvedTargets[i].Format;
            const ResolvedRenderTarget target = viaViewport ? m_Targets.Resolve(VIEWPORT_TARGET)
                                                            : view.ResolvedTargets[i];
            if (target.Status != RenderTargetStatus::Ok)
            {
                ReportOnce("FrameRenderer: view " + std::to_string(view.Id) + ": " + target.Message);
                return std::nullopt;
            }
            if (isMain)
                source = viaViewport ? PresentSource::Viewport : PresentSource::Main;
            else if (view.Desc.Targets[i] == VIEWPORT_TARGET)
                source = PresentSource::Viewport;

            const RGTexture imported = graph.ImportTexture(viaViewport ? VIEWPORT_TARGET : view.Desc.Targets[i],
                                                           target.Format);
            graph.BindImportedTexture(imported, target.Texture);
            m_OutputTargets.push_back(imported);
            // The graph's output is the target itself: relative to the result, or, for main written
            // directly, to the swapchain, which is the same size.
            const RGSizePolicy size = isMain && !viaViewport ? RGSizePolicy::MakeRelativeToSwapchain(1.0f)
                                                             : RGSizePolicy::MakeRelativeToResult(1.0f);
            m_OutputContract.push_back({ target.Format, size });
        }

        const GraphInstantiationResult result = m_Instantiator.Instantiate(*asset, graph, &m_OutputContract,
                                                                           &shared.Imports, &m_OutputTargets);
        if (!result.Success)
        {
            std::string message = "FrameRenderer: view " + std::to_string(view.Id) + " (graph '"
                                + view.Desc.GraphName + "') could not be instantiated:";
            for (const GraphInstantiationError& error : result.Errors)
                message += "\n  " + error.Message;
            ReportOnce(message);
            return std::nullopt;
        }

        // Record what this view wrote, for the views after it that read these targets. Its output
        // slots are the last ones declared.
        const std::vector<RGOutputSlot>& slots = graph.GetDescription().OutputSlots;
        const size_t                     first = slots.size() - asset->Outputs.size();
        for (size_t i = 0; i < asset->Outputs.size() && i < view.Desc.Targets.size(); ++i)
        {
            const RGOutputSlot& slot = slots[first + i];
            if (slot.IsBound)
                m_WrittenTargets[view.Desc.Targets[i]] = RGTexture{ slot.BoundResource, slot.BoundVersion };
        }
        return source;
    }

    void FrameRenderer::Present(const FrameSync& sync, uint32_t imageIndex, PresentSource source)
    {
        RHI::IRHICommandList&      cmd      = sync.Cmd;
        RHI::IRHITexture&          image    = m_Swapchain.GetTexture(imageIndex);
        const ResolvedRenderTarget viewport = m_Targets.Resolve(VIEWPORT_TARGET);

        if (source == PresentSource::Main)
        {
            // The graph wrote the swapchain image itself and left it a colour attachment.
            cmd.TransitionTexture(image, RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::Present);
        }
        else if (source == PresentSource::Viewport && viewport.Status == RenderTargetStatus::Ok)
        {
            // The graph left the viewport as a colour attachment, its last write.
            cmd.TransitionTexture(*viewport.Texture, RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::TransferSrc);
            cmd.TransitionTexture(image, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
            cmd.CopyTextureToTexture(*viewport.Texture, image);
            cmd.TransitionTexture(image, RHI::ImageLayout::TransferDst, RHI::ImageLayout::Present);
        }
        else
        {
            // Nothing rendered the viewport: present a cleared frame (RENDERING.md section 8).
            ReportOnce("FrameRenderer: no view presented this frame; presenting a cleared frame.");
            RHI::RenderingAttachment clear;
            clear.Texture     = &image;
            clear.LoadOp      = RHI::LoadOp::Clear;
            clear.StoreOp     = RHI::StoreOp::Store;
            clear.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

            RHI::RenderingInfo info;
            info.ColorAttachments = { clear };
            info.Width            = image.GetWidth();
            info.Height           = image.GetHeight();

            cmd.TransitionTexture(image, RHI::ImageLayout::Undefined, RHI::ImageLayout::ColorAttachment);
            cmd.BeginRendering(info);
            cmd.EndRendering();
            cmd.TransitionTexture(image, RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::Present);
        }

        cmd.End();
        m_Device.SubmitCommandList(cmd, { &sync.ImageAvailable }, { &sync.RenderFinished }, &sync.Fence);
        m_Swapchain.Present(imageIndex, sync.RenderFinished);
    }

    bool FrameRenderer::Presents(const View& view)
    {
        const auto& targets = view.Desc.Targets;
        return std::find(targets.begin(), targets.end(), VIEWPORT_TARGET) != targets.end()
            || std::find(targets.begin(), targets.end(), RenderTargetRegistry::MAIN_TARGET) != targets.end();
    }

    // A problem that persists would otherwise be logged every frame.
    void FrameRenderer::ReportOnce(const std::string& message)
    {
        if (message == m_LastReport)
            return;
        m_LastReport = message;
        LOGWARNING(message);
    }
}
