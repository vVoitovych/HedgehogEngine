#include "InitPass.hpp"

#include "RenderGraph/RenderGraphTypes.hpp"

#include "Profiling/Profiler.hpp"

#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHICommandList.hpp"

namespace Renderer
{
    void InitPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("InitPass");

        ctx.Fence->Wait();

        ctx.BackBufferIndex = ctx.Swapchain->AcquireNextImage(*ctx.ImageAvailableSemaphore);

        ctx.Fence->Reset();

        ctx.CommandList->Reset();
        ctx.CommandList->Begin();
    }
}
