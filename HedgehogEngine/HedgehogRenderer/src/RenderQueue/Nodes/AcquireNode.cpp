#include "AcquireNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/InitPass/InitPass.hpp"

namespace Renderer
{
    AcquireNode::AcquireNode()
        : m_Pass(std::make_unique<InitPass>())
    {}

    AcquireNode::~AcquireNode() = default;

    void AcquireNode::Render(RenderContext& ctx)
    {
        ctx.m_BackBufferIndex = m_Pass->Render(
            *ctx.m_Swapchain,
            *ctx.m_Fence,
            *ctx.m_ImageAvailable,
            *ctx.m_Cmd);
    }

    void AcquireNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }

} // namespace Renderer
