#pragma once

#include "../IRenderNode.hpp"

#include <memory>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class ForwardPass;

    class ForwardPassNode final : public IRenderNode
    {
    public:
        // resourceManager must be non-const: ForwardPass injects the material
        // descriptor set layout into ResourceRegistry during construction.
        ForwardPassNode(RHI::IRHIDevice& device, ResourceManager& resourceManager);
        ~ForwardPassNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;
        void OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm) override;

    private:
        std::unique_ptr<ForwardPass> m_Pass;
    };

} // namespace Renderer
