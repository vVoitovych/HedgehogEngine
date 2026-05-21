#include "DepthPrepassNode.hpp"

#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"

namespace Renderer
{
    DepthPrepassNode::DepthPrepassNode(RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<DepthPrePass>(device, resourceManager))
    {}

    void DepthPrepassNode::Execute(NodeContext& ctx)
    {
        m_Pass->Render(ctx.frame, ctx.resourceManager, ctx.cmd, ctx.frameIndex);
    }

    void DepthPrepassNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void DepthPrepassNode::OnSceneViewResize(RHI::IRHIDevice& device,
                                             const ResourceManager& resourceManager)
    {
        m_Pass->ResizeResources(device, resourceManager);
    }
}
