#include "Renderer.hpp"

#include "HedgehogEngine/Context/HedgehogEngine.hpp"
#include "HedgehogEngine/Context/WindowContext.hpp"
#include "HedgehogEngine/Context/EngineContext.hpp"

#include "HedgehogRenderer/RHIContext/RHIContext.hpp"
#include "HedgehogRenderer/ThreadContext/ThreadContext.hpp"
#include "HedgehogRenderer/RenderQueue/RenderQueue.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    Renderer::Renderer(HedgehogEngine::HedgehogEngine& context)
    {
        auto& windowContext = context.GetWindowContext();
        m_RHIContext    = std::make_unique<RHIContext>(windowContext);
        m_ThreadContext = std::make_unique<ThreadContext>(m_RHIContext->GetRHIDevice());
        m_ResourceManager = std::make_unique<ResourceManager>(
            m_RHIContext->GetRHIDevice(),
            m_RHIContext->GetRHISwapchain(),
            context.GetEngineContext().GetSettings());
        m_RenderQueue = std::make_unique<RenderQueue>(
            m_RHIContext->GetRHIDevice(),
            windowContext.GetWindow(),
            context.GetEngineContext().GetSettings(),
            *m_ResourceManager);
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Cleanup(HedgehogEngine::HedgehogEngine& context)
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

    void Renderer::DrawFrame(HedgehogEngine::HedgehogEngine& context)
    {
        auto& windowContext = context.GetWindowContext();
        auto& engineContext = context.GetEngineContext();
        auto& window        = windowContext.GetWindow();
        auto& device        = m_RHIContext->GetRHIDevice();
        auto& swapchain     = m_RHIContext->GetRHISwapchain();

        m_ResourceManager->SyncResources(device, engineContext);

        const auto&    frameData  = engineContext.GetFrameData();
        const uint32_t frameIndex = m_ThreadContext->GetFrameIndex();

        m_RenderQueue->UpdateData(frameData, frameIndex, engineContext.GetSettings());

        if (windowContext.IsWindowResized() || window.IsResized())
        {
            m_RenderQueue->DiscardGui();

            if (window.IsResized())
                window.ResetResizedFlag();
            windowContext.ResetWindowResizeState();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(windowContext);

            m_ResourceManager->ResizeFrameBufferSizeDependenteResources(device, swapchain);
            m_RenderQueue->ResizeResources(device, *m_ResourceManager);

            return;
        }

        if (engineContext.GetSettings().IsDirty())
        {
            m_ResourceManager->ResizeSettingsDependenteResources(device, engineContext.GetSettings());
            m_RenderQueue->UpdateResources(device, engineContext.GetSettings(), *m_ResourceManager);
            engineContext.GetSettings().CleanDirtyState();
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
    }
}
