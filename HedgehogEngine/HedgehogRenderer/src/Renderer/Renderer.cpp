#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "ResourceManager/ResourceNames.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHITypes.hpp"

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
        const auto& scene = m_ResourceManager->GetTexture(ResourceNames::SceneColorBuffer);
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

        m_RenderQueue->PreRender(frameData, frameIndex, engineContext.GetSettings());

        if (windowContext.IsWindowResized() || window.IsResized())
        {
            m_RenderQueue->DiscardGui();

            if (window.IsResized())
                window.ResetResizedFlag();
            windowContext.ResetWindowResizeState();

            device.WaitIdle();
            m_RHIContext->RecreateSwapchain(windowContext);

            m_ResourceManager->ResizeFrameBufferSizeDependentResources(device, swapchain);

            return;
        }

        if (engineContext.GetSettings().IsDirty())
        {
            m_ResourceManager->ResizeSettingsDependentResources(device, engineContext.GetSettings());
            engineContext.GetSettings().CleanDirtyState();
        }

        // Pre-frame: wait for GPU, acquire swapchain image, begin recording.
        auto& cmd            = m_ThreadContext->GetCommandList();
        auto& fence          = m_ThreadContext->GetFence();
        auto& imageAvailable = m_ThreadContext->GetImageAvailableSemaphore();
        auto& renderFinished = m_ThreadContext->GetRenderFinishedSemaphore();

        fence.Wait();
        const uint32_t backBufferIndex = swapchain.AcquireNextImage(imageAvailable);
        fence.Reset();
        cmd.Reset();
        cmd.Begin();

        m_RenderQueue->Render(frameData, device, cmd, frameIndex, *m_ResourceManager);

        // Post-frame: blit color buffer to swapchain, submit, present.
        auto& colorBuffer    = m_ResourceManager->GetTexture(ResourceNames::RHIColorBuffer);
        auto& swapchainImage = swapchain.GetTexture(backBufferIndex);

        cmd.TransitionTexture(colorBuffer,    RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::TransferSrc);
        cmd.TransitionTexture(swapchainImage, RHI::ImageLayout::Undefined,       RHI::ImageLayout::TransferDst);
        cmd.CopyTextureToTexture(colorBuffer, swapchainImage);
        cmd.TransitionTexture(swapchainImage, RHI::ImageLayout::TransferDst,     RHI::ImageLayout::Present);
        cmd.End();

        device.SubmitCommandList(cmd, {&imageAvailable}, {&renderFinished}, &fence);
        swapchain.Present(backBufferIndex, renderFinished);

        m_ThreadContext->NextFrame();

        // Apply pending scene view resize at end of frame so the current frame's
        // ImGui draw data (which references the old descriptor) has already been submitted.
        if (m_DesiredSceneW > 0 && m_DesiredSceneH > 0)
        {
            const auto& sceneBuffer = m_ResourceManager->GetTexture(ResourceNames::SceneColorBuffer);
            if (sceneBuffer.GetWidth() != m_DesiredSceneW || sceneBuffer.GetHeight() != m_DesiredSceneH)
            {
                device.WaitIdle();
                m_ResourceManager->ResizeSceneView(device, m_DesiredSceneW, m_DesiredSceneH);
            }
        }
    }
}
