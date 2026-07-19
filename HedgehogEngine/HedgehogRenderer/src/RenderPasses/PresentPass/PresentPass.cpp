#include "PresentPass.hpp"

#include "Profiling/Profiler.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    void PresentPass::Setup(RenderGraphBuilder& builder)
    {
        m_GuiColorHandle = builder.Read(GraphResourceNames::GUI_COLOR);
    }

    void PresentPass::CreateFramebuffers(RHI::IRHIDevice& /*device*/, RenderGraph& graph)
    {
        m_GuiColorTexture = &graph.GetTexture(m_GuiColorHandle);
    }

    void PresentPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("PresentPass");

        auto& cmd            = *ctx.m_CommandList;
        auto& swapchainImage = ctx.m_Swapchain->GetTexture(ctx.m_BackBufferIndex);

        cmd.TransitionTexture(*m_GuiColorTexture, RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::TransferSrc);
        cmd.TransitionTexture(swapchainImage,     RHI::ImageLayout::Undefined,       RHI::ImageLayout::TransferDst);
        cmd.CopyTextureToTexture(*m_GuiColorTexture, swapchainImage);
        cmd.TransitionTexture(swapchainImage, RHI::ImageLayout::TransferDst, RHI::ImageLayout::Present);

        cmd.End();

        ctx.m_Device->SubmitCommandList(
            cmd,
            { ctx.m_ImageAvailableSemaphore },
            { ctx.m_RenderFinishedSemaphore },
            ctx.m_Fence);

        ctx.m_Swapchain->Present(ctx.m_BackBufferIndex, *ctx.m_RenderFinishedSemaphore);
    }

    void PresentPass::Cleanup(RHI::IRHIDevice& /*device*/)
    {
    }
}
