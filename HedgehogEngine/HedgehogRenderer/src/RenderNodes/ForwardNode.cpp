#include "ForwardNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"

namespace Renderer
{
    ForwardNode::ForwardNode(RHI::IRHIDevice& device, ResourceManager& resourceManager)
        : m_Pass(std::make_unique<ForwardPass>(device, resourceManager))
    {}

    void ForwardNode::Execute(RenderContext& ctx)
    {
        m_Pass->Render(ctx.GetFrameData(), ctx.GetResourceManager(),
                       ctx.GetCommandList(), ctx.GetFrameIndex());
    }

    void ForwardNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void ForwardNode::OnSceneViewResize(RHI::IRHIDevice& device,
                                        const ResourceManager& resourceManager)
    {
        m_Pass->ResizeResources(device, resourceManager);
    }
}
