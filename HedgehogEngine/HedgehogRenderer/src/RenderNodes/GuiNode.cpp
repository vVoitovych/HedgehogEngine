#include "GuiNode.hpp"

#include "RenderGraph/RenderContext.hpp"
#include "RenderPasses/GuiPass/GuiPass.hpp"
#include "ResourceManager/ResourceManager.hpp"

namespace Renderer
{
    GuiNode::GuiNode(HW::Window& window,
                     RHI::IRHIDevice& device,
                     const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<GuiPass>(window, device, resourceManager))
    {}

    void GuiNode::Setup(RenderGraph& /*graph*/)
    {
        m_Desc.name   = "GuiNode";
        m_Desc.inputs = {
            { "SceneColorBuffer", ResourceType::Color, ResourceAccess::Read,
              RHI::ImageLayout::ShaderReadOnly }
        };
    }

    void GuiNode::Execute(RenderContext& ctx)
    {
        // Barrier (ColorAttachment → ShaderReadOnly) is emitted by RenderGraph::Execute().
        m_Pass->Render(ctx.GetCommandList(), ctx.GetResourceManager());
    }

    void GuiNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void GuiNode::OnWindowResize(RHI::IRHIDevice& device,
                                 const ResourceManager& resourceManager)
    {
        m_Pass->ResizeResources(device, resourceManager);
    }

    void GuiNode::OnSceneViewResize(RHI::IRHIDevice& /*device*/,
                                    const ResourceManager& resourceManager)
    {
        m_Pass->RecreateSceneDescriptor(resourceManager);
    }

    void GuiNode::BeginFrame()
    {
        m_Pass->BeginFrame();
    }

    void GuiNode::DiscardFrame()
    {
        m_Pass->DiscardFrame();
    }

    void* GuiNode::GetSceneViewTextureId() const
    {
        return m_Pass->GetSceneViewTextureId();
    }
}
