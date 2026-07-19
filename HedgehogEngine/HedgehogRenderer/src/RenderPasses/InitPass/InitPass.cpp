#include "InitPass.hpp"

#include "Profiling/Profiler.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHICommandList.hpp"

namespace Renderer
{
    void InitPass::Setup(RenderGraphBuilder& /*builder*/)
    {
    }

    void InitPass::CreateFramebuffers(RHI::IRHIDevice& /*device*/, RenderGraph& /*graph*/)
    {
    }

    void InitPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("InitPass");

        ctx.m_Fence->Wait();

        ctx.m_BackBufferIndex = ctx.m_Swapchain->AcquireNextImage(*ctx.m_ImageAvailableSemaphore);

        ctx.m_Fence->Reset();

        ctx.m_CommandList->Reset();
        ctx.m_CommandList->Begin();
    }

    void InitPass::Cleanup(RHI::IRHIDevice& /*device*/)
    {
    }
}
