#include "GuiNode.hpp"

#include "RenderPasses/GuiPass/GuiPass.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    GuiNode::GuiNode(HW::Window& window,
                     RHI::IRHIDevice& device,
                     const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<GuiPass>(window, device, resourceManager))
    {}

    void GuiNode::Execute(NodeContext& ctx)
    {
        // Transition SceneColorBuffer from write to read before GuiPass samples it.
        auto& sceneBuffer = const_cast<RHI::IRHITexture&>(ctx.resourceManager.GetSceneColorBuffer());
        ctx.cmd.TransitionTexture(sceneBuffer,
            RHI::ImageLayout::ColorAttachment,
            RHI::ImageLayout::ShaderReadOnly);

        m_Pass->Render(ctx.cmd, ctx.resourceManager);
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
