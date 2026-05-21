#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class DepthPrePass;

    class DepthPrepassNode : public IRenderNode
    {
    public:
        DepthPrepassNode(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        void Execute(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        void OnSceneViewResize(RHI::IRHIDevice& device,
                               const ResourceManager& resourceManager) override;

    private:
        std::unique_ptr<DepthPrePass> m_Pass;
    };
}
