#include "HedgehogRenderer/Renderer.hpp"

#include "FrameRenderer.hpp"
#include "Profiling/Profiler.hpp"
#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/RHIDiagnostics.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <optional>

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
                       const FS::FileSystemManager& fileSystem,
                       RendererPaths paths)
        : m_Window(window)
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());
        m_ResourceManager = std::make_unique<ResourceManager>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            settings);
        if (paths == RendererPaths::LegacyAndRenderGraph)
        {
            m_RenderQueue = std::make_unique<RenderQueue>(
                m_RHIContext->GetRHIDevice(),
                window,
                settings,
                *m_ResourceManager,
                fileSystem);
        }
        m_FrameRenderer = std::make_unique<FrameRenderer>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            fileSystem);
        // Without the legacy ForwardPass, nothing else gives the registry its material layout.
        if (!m_RenderQueue)
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
        if (m_RenderQueue)
            m_RenderQueue->Cleanup(device);
        m_ResourceManager->Cleanup(device);
        m_ThreadContext->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    void Renderer::BeginGui()
    {
        assert(m_RenderQueue && "Renderer::BeginGui: this renderer was built without the legacy path (and ImGui).");
        m_RenderQueue->BeginGui();
    }

    void* Renderer::GetSceneViewTextureId() const
    {
        return m_RenderQueue ? m_RenderQueue->GetSceneViewTextureId() : nullptr;
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
        if (m_RenderQueue)
            m_RenderQueue->GetFrameStats().BeginCapture();
    }

    void Renderer::EndFrameStatsCaptureAndLogReport()
    {
        if (!m_RenderQueue)
            return;
        auto& stats = m_RenderQueue->GetFrameStats();
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

    void Renderer::RenderFrame(const HX::RenderScene& scene, const HedgehogSettings::Settings& settings)
    {
        HH_PROFILE_ZONE("RenderFrame");
        std::optional<ScopedCpuSample> sample;
        if (m_RenderQueue)
        {
            sample.emplace(m_RenderQueue->GetFrameStats(), "RenderFrame(total)");
            // The editor began an ImGui frame; nothing on this path draws it yet.
            m_RenderQueue->DiscardGui();
        }

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ThreadContext->GetFence().Wait();

        // Resize before acquiring, so a resize never costs a frame.
        if (m_Window.IsResized())
        {
            m_Window.ResetResizedFlag();
            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);
            // The legacy resources follow too, so DrawFrame finds them sized for the new window.
            m_ResourceManager->ResizeFrameBufferSizeDependentResources(device, swapchain);
            if (m_RenderQueue)
                m_RenderQueue->ResizeResources(device, *m_ResourceManager);
            m_FrameRenderer->NotifySwapchainResized();
        }

        const FrameSync sync{ m_ThreadContext->GetCommandList(), m_ThreadContext->GetFence(),
                              m_ThreadContext->GetImageAvailableSemaphore(),
                              m_ThreadContext->GetRenderFinishedSemaphore(), m_ThreadContext->GetFrameIndex() };
        m_FrameRenderer->Render(scene, m_ResourceManager->GetResourceRegistry(), settings, sync);

        m_ThreadContext->NextFrame();
        HH_PROFILE_FRAME();
    }

    void Renderer::DrawFrame(const HedgehogEngine::FrameData& frameData,
                             HedgehogEngine::IResourceCatalog& catalog,
                             HedgehogSettings::Settings&       settings)
    {
        HH_PROFILE_ZONE("DrawFrame");
        assert(m_RenderQueue && "Renderer::DrawFrame: this renderer was built without the legacy path.");
        ScopedCpuSample sample(m_RenderQueue->GetFrameStats(), "DrawFrame(total)");

        auto& device    = m_RHIContext->GetRHIDevice();
        auto& swapchain = m_RHIContext->GetRHISwapchain();

        m_ResourceManager->SyncResources(device, catalog);

        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        m_RenderQueue->UpdateData(frameData, frameIndex, settings);

        if (m_Window.IsResized())
        {
            m_RenderQueue->DiscardGui();

            m_Window.ResetResizedFlag();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(m_Window);

            m_ResourceManager->ResizeFrameBufferSizeDependentResources(device, swapchain);
            m_RenderQueue->ResizeResources(device, *m_ResourceManager);

            return;
        }

        if (settings.IsDirty())
        {
            m_ResourceManager->ResizeSettingsDependentResources(device, settings);
            m_RenderQueue->UpdateResources(device, settings, *m_ResourceManager);
            settings.CleanDirtyState();
        }

        m_RenderQueue->Render(
            frameData,
            device,
            swapchain,
            m_ThreadContext->GetCommandList(),
            m_ThreadContext->GetFence(),
            m_ThreadContext->GetImageAvailableSemaphore(),
            m_ThreadContext->GetRenderFinishedSemaphore(),
            frameIndex,
            *m_ResourceManager);

        m_ThreadContext->NextFrame();

        // Apply pending scene view resize at end of frame so the current frame's
        // ImGui draw data (which references the old descriptor) has already been submitted.
        if (m_DesiredSceneW > 0 && m_DesiredSceneH > 0)
        {
            const auto& sceneBuffer = m_ResourceManager->GetSceneColorBuffer();
            if (sceneBuffer.GetWidth() != m_DesiredSceneW || sceneBuffer.GetHeight() != m_DesiredSceneH)
            {
                device.WaitIdle();
                m_ResourceManager->ResizeSceneView(device, m_DesiredSceneW, m_DesiredSceneH);
                m_RenderQueue->ResizeSceneView(device, *m_ResourceManager);
            }
        }

        HH_PROFILE_FRAME();
    }
}
