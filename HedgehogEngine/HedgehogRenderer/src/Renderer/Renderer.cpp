#include "HedgehogRenderer/Renderer.hpp"

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
    {
        m_RHIContext    = std::make_unique<RHIContext>(window);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());
        m_ResourceManager = std::make_unique<ResourceManager>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            settings);
        m_RenderQueue = std::make_unique<RenderQueue>(
            m_RHIContext->GetRHIDevice(),
            window,
            settings,
            *m_ResourceManager,
            fileSystem);
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup()
    {
        auto& device = m_RHIContext->GetRHIDevice();
        device.WaitIdle();
        m_RenderQueue->Cleanup(device);
        m_ResourceManager->Cleanup(device);
        m_ThreadContext->Cleanup(device);
        m_RHIContext->Cleanup();
    }

    void Renderer::BeginGui()
    {
        m_RenderQueue->BeginGui();
    }

    void* Renderer::GetSceneViewTextureId() const
    {
        return m_RenderQueue->GetSceneViewTextureId();
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

    void* Renderer::GetGameViewTextureId() const
    {
        return m_RenderQueue->GetGameViewTextureId();
    }

    void Renderer::SetGameViewSize(uint32_t width, uint32_t height)
    {
        m_DesiredGameW = width;
        m_DesiredGameH = height;
    }

    void Renderer::SetGameViewVisible(bool visible)
    {
        m_GameViewVisible = visible;
    }

    void Renderer::SetSelectedGizmo(const std::optional<HM::Matrix4x4>& worldMatrix)
    {
        m_SelectedGizmo = worldMatrix;
    }

    void Renderer::BeginFrameStatsCapture()
    {
        m_RenderQueue->GetFrameStats().BeginCapture();
    }

    void Renderer::EndFrameStatsCaptureAndLogReport()
    {
        auto& stats = m_RenderQueue->GetFrameStats();
        stats.EndCapture();
        stats.LogReport();
    }

    void Renderer::DrawFrame(const HedgehogEngine::FrameData& frameData,
                             HedgehogEngine::IResourceCatalog& catalog,
                             HedgehogSettings::Settings&       settings)
    {
        HH_PROFILE_ZONE("DrawFrame");
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
            m_GameViewVisible,
            m_SelectedGizmo,
            *m_ResourceManager);

        m_ThreadContext->NextFrame();

        // Apply pending view resizes at end of frame so the current frame's ImGui draw data
        // (which references the old descriptors) has already been submitted.
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

        if (m_DesiredGameW > 0 && m_DesiredGameH > 0)
        {
            const auto& gameBuffer = m_ResourceManager->GetColorBuffer(RenderTargetId::Game);
            if (gameBuffer.GetWidth() != m_DesiredGameW || gameBuffer.GetHeight() != m_DesiredGameH)
            {
                device.WaitIdle();
                m_ResourceManager->ResizeGameView(device, m_DesiredGameW, m_DesiredGameH);
                m_RenderQueue->ResizeGameView(device, *m_ResourceManager);
            }
        }

        HH_PROFILE_FRAME();
    }
}
