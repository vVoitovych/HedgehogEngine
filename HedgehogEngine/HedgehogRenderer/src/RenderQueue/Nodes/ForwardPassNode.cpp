#include "ForwardPassNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/ForwardPass/ForwardPass.hpp"

namespace Renderer
{
    ForwardPassNode::ForwardPassNode(RHI::IRHIDevice& device, ResourceManager& resourceManager)
        : m_Pass(std::make_unique<ForwardPass>(device, resourceManager))
    {}

    ForwardPassNode::~ForwardPassNode() = default;

    void ForwardPassNode::Render(RenderContext& ctx)
    {
        m_Pass->Render(*ctx.m_FrameData, *ctx.m_ResourceManager, *ctx.m_Cmd, ctx.m_FrameIndex);
    }

    void ForwardPassNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void ForwardPassNode::OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        m_Pass->ResizeResources(device, rm);
    }

} // namespace Renderer
