#include "ForwardNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"
#include "PipelineManager/PipelineManager.hpp"

namespace Renderer
{
    ForwardNode::ForwardNode(RHI::IRHIDevice& device, ResourceManager& resourceManager,
                             ShaderManager& shaderManager, PipelineManager& pipelineManager)
        : m_Pass(std::make_unique<ForwardPass>(device, resourceManager, shaderManager, pipelineManager))
    {}

    void ForwardNode::Setup(RenderGraph& /*graph*/)
    {
        m_Desc.name    = "ForwardNode";
        m_Desc.outputs = {
            { "SceneColorBuffer", ResourceType::Color, ResourceAccess::Write,
              RHI::ImageLayout::ColorAttachment }
        };
    }

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
