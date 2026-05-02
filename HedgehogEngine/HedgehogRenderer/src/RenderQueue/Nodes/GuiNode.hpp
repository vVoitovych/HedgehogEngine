#pragma once

#include "../IRenderNode.hpp"

#include <memory>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class GuiPass;

    class GuiNode final : public IRenderNode
    {
    public:
        GuiNode(HW::Window& window,
                RHI::IRHIDevice& device,
                const ResourceManager& resourceManager);
        ~GuiNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;
        void OnResizeFramebuffer(RHI::IRHIDevice& device, const ResourceManager& rm) override;
        void OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm) override;

        void  BeginFrame();
        void  DiscardFrame();
        void  RecreateSceneDescriptor(const ResourceManager& rm);
        void* GetSceneViewTextureId() const;

    private:
        std::unique_ptr<GuiPass> m_Pass;
    };

} // namespace Renderer
