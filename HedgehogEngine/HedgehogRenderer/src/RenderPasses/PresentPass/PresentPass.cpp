#include "PresentPass.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    void PresentPass::Render(RHI::IRHICommandList& cmd,
                             RHI::IRHIDevice&      device,
                             RHI::IRHISwapchain&   swapchain,
                             RHI::IRHITexture&     colorBuffer,
                             uint32_t              backBufferIndex,
                             RHI::IRHISemaphore&   imageAvailableSemaphore,
                             RHI::IRHISemaphore&   renderFinishedSemaphore,
                             RHI::IRHIFence&       fence)
    {
        auto& swapchainImage = swapchain.GetTexture(backBufferIndex);

        cmd.TransitionTexture(colorBuffer,    RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::TransferSrc);
        cmd.TransitionTexture(swapchainImage, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
        cmd.CopyTextureToTexture(colorBuffer, swapchainImage);
        cmd.TransitionTexture(swapchainImage, RHI::ImageLayout::TransferDst, RHI::ImageLayout::Present);

        cmd.End();

        device.SubmitCommandList(
            cmd,
            { &imageAvailableSemaphore },
            { &renderFinishedSemaphore },
            &fence);

        swapchain.Present(backBufferIndex, renderFinishedSemaphore);
    }

    void PresentPass::Cleanup()
    {
    }
}
