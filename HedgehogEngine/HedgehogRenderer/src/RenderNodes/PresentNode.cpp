#include "PresentNode.hpp"

#include "RenderPasses/PresentPass/PresentPass.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    PresentNode::PresentNode()
        : m_Pass(std::make_unique<PresentPass>())
    {}

    void PresentNode::Execute(NodeContext& ctx)
    {
        auto& colorBuffer = const_cast<RHI::IRHITexture&>(ctx.resourceManager.GetRHIColorBuffer());
        m_Pass->Render(ctx.cmd, ctx.device, ctx.swapchain, colorBuffer,
                       ctx.backBufferIndex,
                       ctx.imageAvailableSemaphore, ctx.renderFinishedSemaphore,
                       ctx.fence);
    }

    void PresentNode::Cleanup(RHI::IRHIDevice&)
    {
        m_Pass->Cleanup();
    }
}
