#include "PresentNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/PresentPass/PresentPass.hpp"

#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    PresentNode::PresentNode()
        : m_Pass(std::make_unique<PresentPass>())
    {}

    void PresentNode::Execute(RenderContext& ctx)
    {
        auto& colorBuffer = const_cast<RHI::IRHITexture&>(*ctx.GetTexture("RHIColorBuffer"));
        m_Pass->Render(ctx.GetCommandList(), ctx.GetDevice(), ctx.GetSwapchain(),
                       colorBuffer, ctx.GetBackBufferIndex(),
                       ctx.GetImageAvailableSemaphore(), ctx.GetRenderFinishedSemaphore(),
                       ctx.GetFence());
    }

    void PresentNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }
}
