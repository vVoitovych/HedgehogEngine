#include "DepthPrepassNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"

namespace Renderer
{
    DepthPrepassNode::DepthPrepassNode(RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<DepthPrePass>(device, resourceManager))
    {}

    void DepthPrepassNode::Execute(RenderContext& ctx)
    {
        m_Pass->Render(ctx.GetFrameData(), ctx.GetResourceManager(),
                       ctx.GetCommandList(), ctx.GetFrameIndex());
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
