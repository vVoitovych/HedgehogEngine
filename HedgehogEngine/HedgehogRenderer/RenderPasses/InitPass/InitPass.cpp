#include "InitPass.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"

#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHICommandList.hpp"

namespace Renderer
{
    InitPass::InitPass(const Context::Context& context)
    {
    }

    void InitPass::Render(Context::Context& context)
    {
        auto& vulkanContext = context.GetVulkanContext();
        auto& frameContext  = context.GetFrameContext();
        auto& threadContext = context.GetThreadContext();

        auto& fence = threadContext.GetFence();
        fence.Wait();

        auto& imageAvailableSemaphore = threadContext.GetImageAvailableSemaphore();
        uint32_t imageIndex = vulkanContext.GetRHISwapchain().AcquireNextImage(imageAvailableSemaphore);
        frameContext.SetBackBufferIndex(imageIndex);

        fence.Reset();

        auto& commandList = threadContext.GetCommandList();
        commandList.Reset();
        commandList.Begin();
    }

    void InitPass::Cleanup(const Context::Context& context)
    {
    }

}

