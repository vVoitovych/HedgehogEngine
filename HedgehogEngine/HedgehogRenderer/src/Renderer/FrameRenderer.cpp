#include "FrameRenderer.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "HedgehogMath/api/Common.hpp"

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

        // Vertical field of view in degrees, a Vulkan-style (Y down) clip space, depth 0..1.
        HM::Matrix4x4 MakeProjection(const HX::RenderCamera& camera, float aspect)
        {
            if (camera.ProjectionType == HX::CameraProjectionType::Orthographic)
            {
                const float halfHeight = camera.OrthoSize * 0.5f;
                const float halfWidth  = halfHeight * aspect;
                return HM::Matrix4x4::Ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.NearPlane,
                                            camera.FarPlane);
            }
            HM::Matrix4x4 proj = HM::Matrix4x4::Perspective(HM::ToRadians(camera.Fov), aspect, camera.NearPlane,
                                                            camera.FarPlane);
            proj[1][1] *= -1.0f;
            return proj;
        }
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
                               const HedgehogSettings::Settings& settings, const FrameSync& sync)
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
        for (const View& view : views)
            m_ViewFrames.push_back(MakeViewFrame(view));
        m_ViewContexts.assign(views.size(), GraphFrameContext{});

        const View*           shadowView  = SelectShadowView(views);
        const GraphFrameData* shadowFrame = shadowView ? &m_ViewFrames[shadowView - views.data()] : nullptr;

        const auto& shadowmap = *settings.GetShadowmapSettings();
        SharedPhaseSettings sharedSettings;
        sharedSettings.ShadowAtlasSize  = shadowmap.GetShadowmapSize();
        sharedSettings.ShadowCasterMask = shadowmap.GetShadowCasterMask();

        RenderGraphRuntime&      graph  = *m_Runtimes[sync.FrameIndex];
        const SharedPhaseOutputs shared =
            m_Shared.Declare(graph, m_Services, shadowFrame, scene.Lights, sharedSettings);

        bool hasViewport = false;
        for (size_t i = 0; i < views.size(); ++i)
        {
            const bool declared = DeclareView(graph, views[i], m_ViewFrames[i], m_ViewContexts[i], shared);
            const auto& targets = views[i].Desc.Targets;
            if (declared && std::find(targets.begin(), targets.end(), VIEWPORT_TARGET) != targets.end())
                hasViewport = true;
        }

        if (!graph.Execute(sync.Cmd))
            hasViewport = false;

        Present(sync, imageIndex, hasViewport);

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
        frame.OpaqueInstances = scene.Instances;
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

    GraphFrameData FrameRenderer::MakeViewFrame(const View& view) const
    {
        GraphFrameData frame = m_SceneFrame;
        if (!view.Desc.Camera || view.ResolvedTargets.empty())
            return frame;

        const HX::RenderCamera& camera = *view.Desc.Camera;
        const TargetExtent&     extent = view.ResolvedTargets[0].Extent;
        const float             aspect = static_cast<float>(extent.Width) / static_cast<float>(extent.Height);

        frame.View        = camera.WorldMatrix.Inverse();
        frame.Proj        = MakeProjection(camera, aspect);
        const HM::Vector4& position = camera.WorldMatrix[3]; // the translation column
        frame.EyePosition = HM::Vector3(position.x(), position.y(), position.z());
        frame.NearPlane   = camera.NearPlane;
        frame.FarPlane    = camera.FarPlane;
        return frame;
    }

    bool FrameRenderer::DeclareView(RenderGraphRuntime& graph, const View& view, GraphFrameData& frame,
                                    GraphFrameContext& context, const SharedPhaseOutputs& shared)
    {
        const GraphAsset* asset = m_Library.Find(view.Desc.GraphName);
        if (!asset)
        {
            ReportOnce("FrameRenderer: view " + std::to_string(view.Id) + " uses the unknown graph '"
                       + view.Desc.GraphName + "'; it is skipped.");
            return false;
        }

        frame.SceneLights = shared.SceneLights;
        context           = { &m_Services, &frame };
        graph.SetFrameContext(&context);

        const TargetExtent resultExtent = view.ResolvedTargets[0].Extent;
        const TargetExtent swapchain    = m_Targets.Resolve(RenderTargetRegistry::MAIN_TARGET).Extent;
        graph.SetSizeReferences(resultExtent.Width, resultExtent.Height, swapchain.Width, swapchain.Height);

        // The view's targets, imported so its graph writes them in place.
        m_OutputTargets.clear();
        m_OutputContract.clear();
        for (size_t i = 0; i < view.ResolvedTargets.size(); ++i)
        {
            const ResolvedRenderTarget& target = view.ResolvedTargets[i];
            const RGTexture imported = graph.ImportTexture(view.Desc.Targets[i], target.Format);
            graph.BindImportedTexture(imported, target.Texture);
            m_OutputTargets.push_back(imported);
            m_OutputContract.push_back({ target.Format, RGSizePolicy::MakeRelativeToResult(1.0f) });
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
            return false;
        }
        return true;
    }

    void FrameRenderer::Present(const FrameSync& sync, uint32_t imageIndex, bool hasViewport)
    {
        RHI::IRHICommandList&      cmd      = sync.Cmd;
        RHI::IRHITexture&          image    = m_Swapchain.GetTexture(imageIndex);
        const ResolvedRenderTarget viewport = m_Targets.Resolve(VIEWPORT_TARGET);

        if (hasViewport && viewport.Status == RenderTargetStatus::Ok)
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
            ReportOnce("FrameRenderer: no view rendered the viewport this frame; presenting a cleared frame.");
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

    // A problem that persists would otherwise be logged every frame.
    void FrameRenderer::ReportOnce(const std::string& message)
    {
        if (message == m_LastReport)
            return;
        m_LastReport = message;
        LOGWARNING(message);
    }
}
