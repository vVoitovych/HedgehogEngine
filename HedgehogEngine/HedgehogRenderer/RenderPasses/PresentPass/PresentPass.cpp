#include "PresentPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    PresentPass::PresentPass(const Context::Context& context)
    {
    }

    void PresentPass::Render(Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& frameContext  = context.GetFrameContext();
        auto& threadContext = context.GetThreadContext();
        auto& vulkanContext = context.GetVulkanContext();

        auto& commandList  = threadContext.GetCommandList();
        auto& rhiDevice    = vulkanContext.GetRHIDevice();
        auto& rhiSwapchain = vulkanContext.GetRHISwapchain();

        const uint32_t backBufferIndex = frameContext.GetBackBufferIndex();

        auto& colorBuffer    = const_cast<RHI::IRHITexture&>(resourceManager.GetRHIColorBuffer());
        auto& swapchainImage = rhiSwapchain.GetTexture(backBufferIndex);

        commandList.TransitionTexture(colorBuffer,    RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferSrc);
        commandList.TransitionTexture(swapchainImage, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
        commandList.CopyTextureToTexture(colorBuffer, swapchainImage);
        commandList.TransitionTexture(swapchainImage, RHI::ImageLayout::TransferDst, RHI::ImageLayout::Present);

        commandList.End();

        auto& imageAvailableSemaphore = threadContext.GetImageAvailableSemaphore();
        auto& renderFinishedSemaphore = threadContext.GetRenderFinishedSemaphore();
        auto& fence                   = threadContext.GetFence();

        rhiDevice.SubmitCommandList(
            commandList,
            { &imageAvailableSemaphore },
            { &renderFinishedSemaphore },
            &fence);

        rhiSwapchain.Present(backBufferIndex, renderFinishedSemaphore);

        if (vulkanContext.GetWindow().IsResized())
        {
            vulkanContext.GetWindow().ResetResizedFlag();
            vulkanContext.ResizeWindow();
        }

        threadContext.NextFrame();
    }

    void PresentPass::Cleanup(const Context::Context& context)
    {
    }

}



