#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace HW
{
    class Window;
}

namespace Renderer
{
    class GuiPass;

    class GuiNode : public IRenderNode
    {
    public:
        GuiNode(HW::Window& window,
                RHI::IRHIDevice& device,
                const ResourceManager& resourceManager);

        void Execute(NodeContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        // Recreates the GuiPass framebuffer for the new swapchain size.
        void OnWindowResize(RHI::IRHIDevice& device,
                            const ResourceManager& resourceManager) override;

        // Recreates the scene viewport ImGui descriptor for the new SceneColorBuffer.
        void OnSceneViewResize(RHI::IRHIDevice& device,
                               const ResourceManager& resourceManager) override;

        void  BeginFrame()                  override;
        void  DiscardFrame()                override;
        void* GetSceneViewTextureId() const override;

    private:
        std::unique_ptr<GuiPass> m_Pass;
    };
}
