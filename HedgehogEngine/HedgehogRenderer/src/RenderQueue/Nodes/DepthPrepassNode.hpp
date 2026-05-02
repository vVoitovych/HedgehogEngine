#pragma once

#include "../IRenderNode.hpp"

#include <memory>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class DepthPrePass;

    class DepthPrepassNode final : public IRenderNode
    {
    public:
        DepthPrepassNode(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        ~DepthPrepassNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;
        void OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm) override;

    private:
        std::unique_ptr<DepthPrePass> m_Pass;
    };

} // namespace Renderer
