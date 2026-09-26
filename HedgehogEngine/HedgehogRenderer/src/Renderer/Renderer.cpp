#include "HedgehogRenderer/Renderer.hpp"

#include "FrameRenderer.hpp"
#include "Profiling/Profiler.hpp"
#include "RHIContext/RHIContext.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHIGuiBackend.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/RHIDiagnostics.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

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
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_ResourceManager = std::make_unique<ResourceManager>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            settings);
        m_FrameRenderer = std::make_unique<FrameRenderer>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            fileSystem);
        // The registry allocates material sets in the forward shader's set-1 layout.
        m_FrameRenderer->ProvideMaterialLayout(m_ResourceManager->GetResourceRegistry());
        static_assert(std::string_view(VIEWPORT_TARGET) == FrameRenderer::VIEWPORT_TARGET);
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup()
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_FrameRenderer.reset();
        m_ResourceManager->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    const RHI::IRHITexture& Renderer::GetSceneViewTexture() const
    {
        return m_ResourceManager->GetSceneColorBuffer();
    }

    std::unique_ptr<RHI::IRHIGuiBackend> Renderer::CreateGuiBackend(bool forRenderGraph) const
    {
        RHI::GuiBackendDesc desc;
        desc.MinImageCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        desc.ImageCount    = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        desc.ColorFormat   = forRenderGraph ? m_RHIContext->GetRHISwapchain().GetFormat()
                                            : m_ResourceManager->GetRHIColorBuffer().GetFormat();
        return m_RHIContext->GetRHIDevice().CreateGuiBackend(desc);
    }

    void Renderer::WaitIdle() const
    {
        m_RHIContext->GetRHIDevice().WaitIdle();
    }

    void Renderer::SetCameraTargetOverride(uint64_t cameraSourceId, std::vector<std::string> targets)
    {
        m_FrameRenderer->GetViewManager().SetTargetOverride(cameraSourceId, std::move(targets));
    }

    RenderTargetResult Renderer::DeclareTarget(const RenderTargetDesc& desc)
    {
        return m_FrameRenderer->DeclareTarget(desc);
    }

    RenderTargetResult Renderer::ResizeTarget(std::string_view name, uint32_t width, uint32_t height)
    {
        return m_FrameRenderer->ResizeTarget(name, width, height);
    }

    RHI::IRHITexture* Renderer::GetTargetTexture(std::string_view name) const
    {
        return m_FrameRenderer->FindTargetTexture(name);
    }

    size_t Renderer::GetLastFramePassCount() const
    {
        return m_FrameRenderer->GetLastFramePassCount();
    }

    float Renderer::GetAspectRatio() const
    {
        const auto& scene = m_ResourceManager->GetSceneColorBuffer();
        return static_cast<float>(scene.GetWidth()) / static_cast<float>(scene.GetHeight());
    }

    void Renderer::SetSceneViewSize(uint32_t width, uint32_t height)
    {
        m_DesiredSceneW = width;
        m_DesiredSceneH = height;
    }

    void Renderer::BeginFrameStatsCapture()
    {
        m_FrameRenderer->GetFrameStats().BeginCapture();
    }

    void Renderer::EndFrameStatsCaptureAndLogReport()
    {
        FrameStats& stats = m_FrameRenderer->GetFrameStats();
        stats.EndCapture();
        stats.LogReport();
    }

    void Renderer::SyncResources(HedgehogEngine::IResourceCatalog& catalog)
    {
        m_ResourceManager->SyncResources(m_RHIContext->GetRHIDevice(), catalog);
    }

    ViewId Renderer::CreateView(ViewDesc desc)
    {
        return m_FrameRenderer->GetViewManager().CreateView(std::move(desc));
    }

    bool Renderer::UpdateView(ViewId id, ViewDesc desc)
    {
        return m_FrameRenderer->GetViewManager().UpdateView(id, std::move(desc));
    }

    void Renderer::DestroyView(ViewId id)
    {
        m_FrameRenderer->GetViewManager().DestroyView(id);
    }

    void Renderer::RenderFrame(const HX::RenderScene& scene, const HedgehogSettings::Settings& settings,
                               const UiCallback& ui)
    {
        HH_PROFILE_ZONE("RenderFrame");
        ScopedCpuSample sample(m_FrameRenderer->GetFrameStats(), "RenderFrame(total)");

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_FrameRenderer->WaitForFrameSlot();

        // Resize before acquiring, so a resize never costs a frame.
        if (m_Window.IsResized())
        {
            m_Window.ResetResizedFlag();
            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);
            // The legacy resources still exist until they are deleted; keep them the window's size.
            m_ResourceManager->ResizeFrameBufferSizeDependentResources(device, swapchain);
            m_FrameRenderer->NotifySwapchainResized();
        }

        m_FrameRenderer->Render(scene, m_ResourceManager->GetResourceRegistry(), settings, ui);
        HH_PROFILE_FRAME();
    }

    // The legacy frame path lost its frame-level work (acquire, UI, submit, present) with InitPass,
    // GuiPass, PresentPass and ThreadContext. Nothing calls it; it is deleted with ResourceManager.
    void Renderer::DrawFrame(const HedgehogEngine::FrameData&, HedgehogEngine::IResourceCatalog&,
                             HedgehogSettings::Settings&, const UiCallback&)
    {
        assert(false && "Renderer::DrawFrame: the legacy frame path is removed; use RenderFrame.");
        LOGERROR("Renderer::DrawFrame: the legacy frame path is removed; use RenderFrame.");
    }
}
