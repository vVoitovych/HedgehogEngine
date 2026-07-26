#include "PresentPass.hpp"

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"
#include "RenderGraph/RenderGraphDesc.hpp"

#include "Profiling/Profiler.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>

namespace Renderer
{
    PresentPass::PresentPass(const NodeDesc& node)
    {
        assert(node.Inputs.size() == 1 && "PresentPass requires exactly one 'presentSource' input");
        const std::string& name = node.Inputs.front().Name;

        if (name.rfind("shared:", 0) == 0)
        {
            m_IsImport   = true;
            m_SourceName = "shared/" + name.substr(7);
        }
        else if (name.rfind("views:", 0) == 0)
        {
            m_IsImport   = true;
            m_SourceName = name.substr(6) + "/" + GraphResourceNames::VIEW_COLOR;
        }
        else
        {
            m_IsImport   = false;
            m_SourceName = name;
        }
    }

    void PresentPass::Setup(RenderGraphBuilder& builder)
    {
        // Dependency-only: PresentPass does its own explicit ColorAttachment -> TransferSrc
        // transition below rather than relying on the graph's ReadSampled auto-barrier.
        if (m_IsImport)
            builder.ImportRead(m_SourceName);
        else
            builder.Read(m_SourceName);
    }

    void PresentPass::CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph)
    {
        (void)device;
        m_ColorBuffer = &graph.GetTexture(m_SourceName);
    }

    void PresentPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("PresentPass");

        auto& swapchainImage = ctx.Swapchain->GetTexture(ctx.BackBufferIndex);

        ctx.CommandList->TransitionTexture(*m_ColorBuffer, RHI::ImageLayout::ColorAttachment, RHI::ImageLayout::TransferSrc);
        ctx.CommandList->TransitionTexture(swapchainImage, RHI::ImageLayout::Undefined, RHI::ImageLayout::TransferDst);
        ctx.CommandList->CopyTextureToTexture(*m_ColorBuffer, swapchainImage);
        ctx.CommandList->TransitionTexture(swapchainImage, RHI::ImageLayout::TransferDst, RHI::ImageLayout::Present);

        ctx.CommandList->End();

        ctx.Device->SubmitCommandList(
            *ctx.CommandList,
            { ctx.ImageAvailableSemaphore },
            { ctx.RenderFinishedSemaphore },
            ctx.Fence);

        ctx.Swapchain->Present(ctx.BackBufferIndex, *ctx.RenderFinishedSemaphore);
    }
}
