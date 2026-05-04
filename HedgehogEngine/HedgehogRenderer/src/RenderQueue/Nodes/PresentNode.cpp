#include "PresentNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/PresentPass/PresentPass.hpp"
#include "../../ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHITexture.hpp"

#include <cassert>

namespace Renderer
{
    PresentNode::PresentNode()
        : m_Pass(std::make_unique<PresentPass>())
    {}

    PresentNode::~PresentNode() = default;

    void PresentNode::Render(RenderContext& ctx)
    {
        assert(ctx.m_BackBufferIndex < ctx.m_Swapchain->GetImageCount()
            && "PresentNode: m_BackBufferIndex not set — AcquireNode must run first");

        auto& colorBuffer = const_cast<RHI::IRHITexture&>(ctx.m_ResourceManager->GetRHIColorBuffer());
        m_Pass->Render(
            *ctx.m_Cmd,
            *ctx.m_Device,
            *ctx.m_Swapchain,
            colorBuffer,
            ctx.m_BackBufferIndex,
            *ctx.m_ImageAvailable,
            *ctx.m_RenderFinished,
            *ctx.m_Fence);
    }

    void PresentNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }

} // namespace Renderer
