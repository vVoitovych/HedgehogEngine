#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogEngine/api/HedgehogEngine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"

#include "RHIContext/RHIContext.hpp"
#include "ThreadContext/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "HedgehogRenderer/IRenderNode.hpp"
#include "HedgehogRenderer/PreRenderContext.hpp"
#include "HedgehogRenderer/RenderContext.hpp"
#include "RHI/api/IRHITexture.hpp"
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

    void Renderer::OnBeginFrame()
    {
        m_RenderQueue->OnBeginFrame();
    }

    RHI::IRHIDevice& Renderer::GetDevice()
    {
        return m_RHIContext->GetRHIDevice();
    }

    const RHI::IRHITexture& Renderer::GetTexture(const std::string& name) const
    {
        return m_ResourceManager->GetTexture(name);
    }

    void Renderer::AppendNode(const std::string& name, std::unique_ptr<IRenderNode> node)
    {
        m_RenderQueue->AppendNode(name, std::move(node));
    }

    void* Renderer::QueryNodeExport(const std::string& nodeName, const std::string& key) const
    {
        return m_RenderQueue->QueryNodeExport(nodeName, key);
    }

    float Renderer::GetAspectRatio() const
    {
        const auto& scene = m_ResourceManager->GetTexture(ResourceNames::SceneColorBuffer);
        return static_cast<float>(scene.GetWidth()) / static_cast<float>(scene.GetHeight());
    }

    void Renderer::SetRenderTargetSize(const std::string& resourceName, uint32_t width, uint32_t height)
    {
        m_PendingResizes[resourceName] = { width, height };
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

        // Handle resize/settings changes before touching frame resources.
        if (windowContext.IsWindowResized() || window.IsResized())
        {
            m_RenderQueue->OnDiscardFrame();

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

        auto& cmd            = m_ThreadContext->GetCommandList();
        auto& fence          = m_ThreadContext->GetFence();
        auto& imageAvailable = m_ThreadContext->GetImageAvailableSemaphore();
        auto& renderFinished = m_ThreadContext->GetRenderFinishedSemaphore();

        // Wait for the GPU to finish this frame slot — safe to modify its resources now.
        fence.Wait();
        const uint32_t backBufferIndex = swapchain.AcquireNextImage(imageAvailable);
        fence.Reset();

        // PreRender: resource creation, framebuffer rebuilds, uniform writes.
        // Runs after fence.Wait() so destroyed resources are no longer in use.
        PreRenderContext preCtx{frameData, device, *m_ResourceManager, engineContext.GetSettings(), frameIndex};
        m_RenderQueue->PreRender(preCtx);

        // Pure command recording.
        cmd.Reset();
        cmd.Begin();

        RenderContext renderCtx{frameData, device, cmd, *m_ResourceManager, frameIndex};
        m_RenderQueue->Render(renderCtx);

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

        // Apply pending render-target resizes. Deferred to after submit so any in-flight
        // descriptors referencing the old textures are no longer in use.
        for (const auto& [name, size] : m_PendingResizes)
        {
            if (name == ResourceNames::SceneColorBuffer)
            {
                const auto& buf = m_ResourceManager->GetTexture(ResourceNames::SceneColorBuffer);
                if (buf.GetWidth() != size.first || buf.GetHeight() != size.second)
                {
                    device.WaitIdle();
                    m_ResourceManager->ResizeSceneView(device, size.first, size.second);
                }
            }
        }
        m_PendingResizes.clear();
    }
}
