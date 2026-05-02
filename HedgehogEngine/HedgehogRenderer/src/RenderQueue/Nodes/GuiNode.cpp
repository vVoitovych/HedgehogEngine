#include "GuiNode.hpp"

#include "../RenderContext.hpp"
#include "../../RenderPasses/GuiPass/GuiPass.hpp"

namespace Renderer
{
    GuiNode::GuiNode(HW::Window& window,
                     RHI::IRHIDevice& device,
                     const ResourceManager& resourceManager)
        : m_Pass(std::make_unique<GuiPass>(window, device, resourceManager))
    {}

    GuiNode::~GuiNode() = default;

    void GuiNode::Render(RenderContext& ctx)
    {
        m_Pass->Render(*ctx.m_Cmd, *ctx.m_ResourceManager);
    }

    void GuiNode::Cleanup(RHI::IRHIDevice& device)
    {
        m_Pass->Cleanup(device);
    }

    void GuiNode::OnResizeFramebuffer(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        m_Pass->ResizeResources(device, rm);
    }

    void GuiNode::OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm)
    {
        m_Pass->RecreateSceneDescriptor(rm);
    }

    void GuiNode::BeginFrame()
    {
        m_Pass->BeginFrame();
    }

    void GuiNode::DiscardFrame()
    {
        m_Pass->DiscardFrame();
    }

    void GuiNode::RecreateSceneDescriptor(const ResourceManager& rm)
    {
        m_Pass->RecreateSceneDescriptor(rm);
    }

    void* GuiNode::GetSceneViewTextureId() const
    {
        return m_Pass->GetSceneViewTextureId();
    }

} // namespace Renderer
