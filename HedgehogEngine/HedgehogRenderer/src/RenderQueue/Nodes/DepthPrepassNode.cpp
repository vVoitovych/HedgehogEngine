#include "DepthPrepassNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/DepthPrepass/DepthPrePass.hpp"

namespace Renderer
{
    DepthPrepassNode::DepthPrepassNode(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<DepthPrePass>(device, resourceManager))
    {}

    DepthPrepassNode::~DepthPrepassNode() = default;

    void DepthPrepassNode::Render(RenderContext& ctx)
    {
        m_Pass->Render(*ctx.m_FrameData, *ctx.m_ResourceManager, *ctx.m_Cmd, ctx.m_FrameIndex);
    }

    void DepthPrepassNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void DepthPrepassNode::OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        m_Pass->ResizeResources(device, rm);
    }

} // namespace Renderer
