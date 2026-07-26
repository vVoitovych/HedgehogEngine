#include "HedgehogRenderer/Renderer.hpp"

#include "Profiling/Profiler.hpp"
#include "Profiling/FrameStats.hpp"

#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"

#include "View/ViewRegistry.hpp"
#include "View/RenderView.hpp"

#include "RenderPipeline/RenderPipeline.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"
#include "RenderGraph/RenderGraphDesc.hpp"
#include "RenderGraph/RenderResourceLedger.hpp"
#include "RenderGraph/RenderGraphLoader.hpp"
#include "RenderGraph/RenderGraphInstantiator.hpp"
#include "RenderGraph/RenderPassRegistry.hpp"

#include "RenderPasses/PassInitContext.hpp"
#include "RenderPasses/PassResourceCache.hpp"
#include "RenderPasses/InitPass/InitPass.hpp"
#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"
#include "RenderPasses/GizmoPass/GizmoPass.hpp"
#include "RenderPasses/GuiPass/GuiPass.hpp"
#include "RenderPasses/PresentPass/PresentPass.hpp"

#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/RHIDiagnostics.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    bool AreValidationLayersEnabled()
    {
        return RHI::AreValidationLayersEnabled();
    }

    uint32_t GetValidationErrorCount()
    {
        return RHI::GetValidationErrorCount();
    }

    uint32_t GetValidationWarningCount()
    {
        return RHI::GetValidationWarningCount();
    }

    Renderer::Renderer(HW::Window& window,
                       const HedgehogSettings::Settings& settings,
                       const FS::FileSystemManager& fileSystem)
        : m_Window(window)
        , m_Settings(settings)
        , m_FileSystem(fileSystem)
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());

        auto& device = m_RHIContext->GetRHIDevice();

        m_ResourceRegistry  = std::make_unique<HR::ResourceRegistry>(device);
        m_PassResourceCache = std::make_unique<PassResourceCache>();
        m_Ledger            = std::make_unique<RenderResourceLedger>();
        m_ViewRegistry      = std::make_unique<ViewRegistry>();
        m_FrameStats        = std::make_unique<FrameStats>();

        // Every pass type a .rgq node can name, keyed by its own GetName(). GuiPass needs the
        // window (for ImGui_ImplGlfw_InitForVulkan), captured here since PassInitContext
        // deliberately carries no Window reference — see PassInitContext.hpp. PresentPass needs
        // the node itself (its blit source is data-driven — see PresentPass.hpp); every other
        // pass ignores the NodeDesc parameter entirely.
        m_PassRegistry = std::make_unique<RenderPassRegistry>();
        m_PassRegistry->Register("InitPass", [](const PassInitContext&, const NodeDesc&) {
            return std::make_unique<InitPass>();
        });
        m_PassRegistry->Register("ShadowmapPass", [](const PassInitContext& init, const NodeDesc&) {
            return std::make_unique<ShadowmapPass>(init);
        });
        m_PassRegistry->Register("DepthPrePass", [](const PassInitContext& init, const NodeDesc&) {
            return std::make_unique<DepthPrePass>(init);
        });
        m_PassRegistry->Register("ForwardPass", [](const PassInitContext& init, const NodeDesc&) {
            return std::make_unique<ForwardPass>(init);
        });
        m_PassRegistry->Register("GizmoPass", [](const PassInitContext& init, const NodeDesc&) {
            return std::make_unique<GizmoPass>(init);
        });
        m_PassRegistry->Register("GuiPass", [this](const PassInitContext& init, const NodeDesc&) {
            return std::make_unique<GuiPass>(m_Window, init.Device);
        });
        m_PassRegistry->Register("PresentPass", [](const PassInitContext&, const NodeDesc& node) {
            return std::make_unique<PresentPass>(node);
        });
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup()
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();

        for (const ViewHandle handle : m_ViewRegistry->GetOrderedHandles())
            m_ViewRegistry->Get(handle)->Cleanup(device);

        if (m_CompositionPipeline)
            m_CompositionPipeline->Cleanup(device);
        if (m_FramePipeline)
            m_FramePipeline->Cleanup(device);

        // Last: every pass above has already released its own shared_ptr into the cache, so this
        // drops the final reference and each shared resource's RHI destructor actually runs.
        m_PassResourceCache->Cleanup(device);

        m_ResourceRegistry->Cleanup(device);

        m_ThreadContext->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    void Renderer::BeginGui()
    {
        if (m_CompositionPipeline)
            m_CompositionPipeline->BeginFrame();
    }

    bool Renderer::SetFramePipeline(const std::string& assetPath)
    {
        auto& device = m_RHIContext->GetRHIDevice();

        const auto desc = RenderGraphLoader::Load(assetPath, GraphStage::Frame, *m_PassRegistry, m_FileSystem);
        if (!desc)
            return false;

        const PassInitContext initCtx{ device, m_FileSystem, m_Settings, *m_ResourceRegistry, *m_PassResourceCache };

        std::vector<std::unique_ptr<IRenderPass>> passes;
        if (!RenderGraphInstantiator::Instantiate(*desc, *m_PassRegistry, initCtx, passes))
            return false;

        if (m_FramePipeline)
        {
            device.WaitIdle();
            m_FramePipeline->Cleanup(device);
        }

        m_FramePipeline = std::make_unique<RenderPipeline>(*m_Ledger);
        m_FramePipeline->GetGraph().SetScope("shared");
        for (auto& pass : passes)
            m_FramePipeline->AddPass(std::move(pass));

        const auto& swapchain = m_RHIContext->GetRHISwapchain();
        m_FramePipeline->Compile(device, swapchain.GetWidth(), swapchain.GetHeight(), 0, 0);

        return true;
    }

    bool Renderer::SetCompositionPipeline(const std::string& assetPath)
    {
        auto& device = m_RHIContext->GetRHIDevice();

        const auto desc = RenderGraphLoader::Load(assetPath, GraphStage::Composition, *m_PassRegistry, m_FileSystem);
        if (!desc)
            return false;

        const PassInitContext initCtx{ device, m_FileSystem, m_Settings, *m_ResourceRegistry, *m_PassResourceCache };

        std::vector<std::unique_ptr<IRenderPass>> passes;
        if (!RenderGraphInstantiator::Instantiate(*desc, *m_PassRegistry, initCtx, passes))
            return false;

        if (m_CompositionPipeline)
        {
            device.WaitIdle();
            m_CompositionPipeline->Cleanup(device);
        }

        // Distinct scope from the frame graph's "shared" — Cleanup()'s RenderResourceLedger::Erase
        // is prefix-based, so two graphs sharing a scope would each wipe the other's still-live
        // ledger entries if ever cleaned up independently.
        m_CompositionPipeline = std::make_unique<RenderPipeline>(*m_Ledger);
        m_CompositionPipeline->GetGraph().SetScope("composition");
        for (auto& pass : passes)
            m_CompositionPipeline->AddPass(std::move(pass));

        // Called last (by convention — see ViewDesc/CreateView) so GuiPass, if this pipeline has
        // one, sees the final view set.
        m_CompositionPipeline->NotifyViewsChanged(m_ViewRegistry->GetAllViewOutputNames());

        const auto& swapchain = m_RHIContext->GetRHISwapchain();
        m_CompositionPipeline->Compile(device, swapchain.GetWidth(), swapchain.GetHeight(), 0, 0);

        return true;
    }

    std::optional<ViewHandle> Renderer::CreateView(const ViewDesc& desc)
    {
        auto& device = m_RHIContext->GetRHIDevice();

        const auto loaded = RenderGraphLoader::Load(desc.PipelineAsset, GraphStage::View, *m_PassRegistry, m_FileSystem);
        if (!loaded)
            return std::nullopt;

        const PassInitContext initCtx{ device, m_FileSystem, m_Settings, *m_ResourceRegistry, *m_PassResourceCache };

        std::vector<std::unique_ptr<IRenderPass>> passes;
        if (!RenderGraphInstantiator::Instantiate(*loaded, *m_PassRegistry, initCtx, passes))
            return std::nullopt;

        const ViewHandle handle = m_ViewRegistry->CreateView(desc.Name, *m_Ledger);
        RenderView* view        = m_ViewRegistry->Get(handle);

        // First-created view gets no stats suffix; every later view's pass timings get
        // "[name]" so they don't collide in the per-pass FrameStats table — see
        // workflow/current-plan.md, "Profiling identity". Purely a naming convention, decoupled
        // from SetMainView (which only controls whose camera the shadow pass fits to).
        if (m_ViewRegistry->GetOrderedHandles().size() > 1)
            view->SetStatsSuffix("[" + desc.Name + "]");

        for (auto& pass : passes)
            view->AddPass(std::move(pass));

        view->SetDesiredSize(desc.Width, desc.Height);
        view->Compile(device);

        return handle;
    }

    void Renderer::DestroyView(ViewHandle handle)
    {
        RenderView* view = m_ViewRegistry->Get(handle);
        if (!view)
            return;

        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        view->Cleanup(device);
        m_ViewRegistry->DestroyView(handle);
    }

    std::optional<ViewHandle> Renderer::FindView(const std::string& name) const
    {
        return m_ViewRegistry->FindByName(name);
    }

    void Renderer::SetMainView(ViewHandle handle)
    {
        m_MainView = handle;
    }

    void Renderer::SetViewSize(ViewHandle handle, uint32_t width, uint32_t height)
    {
        if (RenderView* view = m_ViewRegistry->Get(handle))
            view->SetDesiredSize(width, height);
    }

    void Renderer::SetViewEnabled(ViewHandle handle, bool enabled)
    {
        if (RenderView* view = m_ViewRegistry->Get(handle))
            view->SetEnabled(enabled);
    }

    void Renderer::SetViewCamera(ViewHandle handle, const std::optional<HedgehogEngine::CameraData>& camera)
    {
        if (RenderView* view = m_ViewRegistry->Get(handle))
            view->SetCamera(camera);
    }

    void Renderer::SetViewGizmo(ViewHandle handle, const std::optional<HM::Matrix4x4>& worldMatrix)
    {
        if (RenderView* view = m_ViewRegistry->Get(handle))
            view->SetGizmo(worldMatrix);
    }

    void* Renderer::GetViewTextureId(ViewHandle handle) const
    {
        const RenderView* view = m_ViewRegistry->Get(handle);
        if (!view || !m_CompositionPipeline)
            return nullptr;
        return m_CompositionPipeline->GetViewTextureId(view->OutputColorName());
    }

    float Renderer::GetViewAspectRatio(ViewHandle handle) const
    {
        const RenderView* view = m_ViewRegistry->Get(handle);
        if (!view || view->GetCompiledHeight() == 0)
            return 1.0f;
        return static_cast<float>(view->GetCompiledWidth()) / static_cast<float>(view->GetCompiledHeight());
    }

    void Renderer::BeginFrameStatsCapture()
    {
        m_FrameStats->BeginCapture();
    }

    void Renderer::EndFrameStatsCaptureAndLogReport()
    {
        m_FrameStats->EndCapture();
        m_FrameStats->LogReport();
    }

    void Renderer::DrawFrame(const HedgehogEngine::FrameData& frameData,
                             HedgehogEngine::IResourceCatalog& catalog,
                             HedgehogSettings::Settings&       settings)
    {
        // Nothing to render until the application has set both pipelines (see SetFramePipeline /
        // SetCompositionPipeline) — logged once at the call site on failure, not here every frame.
        if (!m_FramePipeline || !m_CompositionPipeline)
            return;

        HH_PROFILE_ZONE("DrawFrame");
        ScopedCpuSample sample(*m_FrameStats, "DrawFrame(total)");

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ResourceRegistry->SyncMeshes(catalog, device);
        m_ResourceRegistry->SyncMaterials(catalog, device);

        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        if (m_Window.IsResized())
        {
            m_CompositionPipeline->DiscardFrame();

            m_Window.ResetResizedFlag();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);

            m_CompositionPipeline->GetGraph().SetSwapchainSize(swapchain.GetWidth(), swapchain.GetHeight());
            m_CompositionPipeline->GetGraph().Invalidate(SizeClass::SwapchainRelative, device);

            return;
        }

        if (settings.IsDirty())
        {
            m_FramePipeline->NotifySettingsDirty(device, settings);
            settings.CleanDirtyState();
        }

        RenderGraphContext ctx;
        ctx.FrameData               = &frameData;
        ctx.FrameIndex              = frameIndex;
        ctx.Device                  = &device;
        ctx.CommandList             = &m_ThreadContext->GetCommandList();
        ctx.Swapchain               = &swapchain;
        ctx.Fence                   = &m_ThreadContext->GetFence();
        ctx.ImageAvailableSemaphore = &m_ThreadContext->GetImageAvailableSemaphore();
        ctx.RenderFinishedSemaphore = &m_ThreadContext->GetRenderFinishedSemaphore();
        ctx.ResourceRegistry        = m_ResourceRegistry.get();
        ctx.Settings                = &settings;

        // Frame stage: InitPass acquires the swapchain image; ShadowmapPass fits cascades to the
        // main view's camera (ctx.View repointed here — see RenderGraphTypes.hpp, View).
        RenderView* mainView = m_ViewRegistry->Get(m_MainView);
        ctx.View = mainView ? &mainView->GetFrameData() : nullptr;
        m_FramePipeline->GetGraph().Update(ctx);
        m_FramePipeline->GetGraph().Execute(ctx, *m_FrameStats);

        // View stage: each enabled view's own graph, in registration order.
        for (const ViewHandle handle : m_ViewRegistry->GetOrderedHandles())
        {
            RenderView* view = m_ViewRegistry->Get(handle);
            if (!view->IsEnabled())
                continue;

            ctx.View = &view->GetFrameData();
            view->GetGraph().Update(ctx);
            view->GetGraph().Execute(ctx, *m_FrameStats);
        }

        // Composition stage: GUI (samples every view's colour output) + present. Not tied to any
        // one view.
        ctx.View = nullptr;
        m_CompositionPipeline->GetGraph().Execute(ctx, *m_FrameStats);

        m_ThreadContext->NextFrame();

        // Apply pending per-view resizes at end of frame so the current frame's already-submitted
        // ImGui draw data (which references the old descriptors) isn't invalidated mid-frame.
        bool anyViewResized = false;
        for (const ViewHandle handle : m_ViewRegistry->GetOrderedHandles())
        {
            RenderView* view = m_ViewRegistry->Get(handle);
            if (view->GetDesiredWidth() == view->GetCompiledWidth() &&
                view->GetDesiredHeight() == view->GetCompiledHeight())
                continue;

            device.WaitIdle();
            view->GetGraph().SetViewSize(view->GetDesiredWidth(), view->GetDesiredHeight());
            view->GetGraph().Invalidate(SizeClass::ViewRelative, device);
            view->MarkCompiled(view->GetDesiredWidth(), view->GetDesiredHeight());
            anyViewResized = true;
        }

        if (anyViewResized)
            m_CompositionPipeline->GetGraph().RefreshImports(device);

        HH_PROFILE_FRAME();
    }
}
