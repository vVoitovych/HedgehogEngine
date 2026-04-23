#include "InitPass.hpp"

#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHICommandList.hpp"

namespace Renderer
{
    uint32_t InitPass::Render(RHI::IRHISwapchain&  swapchain,
                              RHI::IRHIFence&      fence,
                              RHI::IRHISemaphore&  imageAvailableSemaphore,
                              RHI::IRHICommandList& cmd)
    {
        fence.Wait();

        const uint32_t imageIndex = swapchain.AcquireNextImage(imageAvailableSemaphore);

        fence.Reset();

        cmd.Reset();
        cmd.Begin();

        return imageIndex;
    }

    void InitPass::Cleanup()
    {
    }
}
