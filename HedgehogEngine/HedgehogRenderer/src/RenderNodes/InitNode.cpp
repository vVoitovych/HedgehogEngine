#include "InitNode.hpp"

#include "RenderPasses/InitPass/InitPass.hpp"

namespace Renderer
{
    InitNode::InitNode()
        : m_Pass(std::make_unique<InitPass>())
    {}

    void InitNode::Execute(NodeContext& ctx)
    {
        ctx.backBufferIndex = m_Pass->Render(
            ctx.swapchain, ctx.fence, ctx.imageAvailableSemaphore, ctx.cmd);
    }

    void InitNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }
}
