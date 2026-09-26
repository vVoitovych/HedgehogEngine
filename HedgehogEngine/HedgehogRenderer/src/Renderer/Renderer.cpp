#include "HedgehogRenderer/Renderer.hpp"

#include "FrameRenderer.hpp"
#include "Profiling/Profiler.hpp"
#include "RHIContext/RHIContext.hpp"
#include "ResourceRegistry/ResourceRegistry.hpp"

#include "HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/RHIDiagnostics.hpp"

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

    Renderer::Renderer(HW::Window& window, const FS::FileSystemManager& fileSystem,
                       const DeviceReadyCallback& onDeviceReady)
        : m_Window(window)
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_Resources     = std::make_unique<HR::ResourceRegistry>(m_RHIContext->GetRHIDevice());
        m_FrameRenderer = std::make_unique<FrameRenderer>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            fileSystem);
        // The registry allocates material sets in the forward shader's set-1 layout.
        m_FrameRenderer->ProvideMaterialLayout(*m_Resources);

        if (onDeviceReady)
            onDeviceReady({ m_RHIContext->GetRHIDevice(), m_RHIContext->GetRHISwapchain().GetFormat() });
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup()
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_FrameRenderer.reset();
        m_Resources->Cleanup(device);
        m_Resources.reset();
        m_RHIContext->Cleanup();
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
        auto& device = m_RHIContext->GetRHIDevice();
        m_Resources->SyncMeshes(catalog, device);
        m_Resources->SyncMaterials(catalog, device);
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

        m_FrameRenderer->WaitForFrameSlot();

        // Resize before acquiring, so a resize never costs a frame.
        if (m_Window.IsResized())
        {
            m_Window.ResetResizedFlag();
            m_RHIContext->GetRHIDevice().WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);
            m_FrameRenderer->NotifySwapchainResized();
        }

        m_FrameRenderer->Render(scene, *m_Resources, settings, ui);
        HH_PROFILE_FRAME();
    }
}
