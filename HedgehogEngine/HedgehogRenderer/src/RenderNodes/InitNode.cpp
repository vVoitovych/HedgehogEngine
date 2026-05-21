#include "InitNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/InitPass/InitPass.hpp"

namespace Renderer
{
    InitNode::InitNode()
        : m_Pass(std::make_unique<InitPass>())
    {}

    void InitNode::Execute(RenderContext& ctx)
    {
        ctx.SetBackBufferIndex(
            m_Pass->Render(ctx.GetSwapchain(), ctx.GetFence(),
                           ctx.GetImageAvailableSemaphore(), ctx.GetCommandList()));
    }

    void InitNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }
}
