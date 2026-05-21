#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class ForwardPass;

    class ForwardNode : public IRenderNode
    {
    public:
        // Non-const ResourceManager& required: ForwardPass constructor injects
        // the material descriptor layout into ResourceRegistry on construction.
        ForwardNode(RHI::IRHIDevice& device, ResourceManager& resourceManager);

        void Execute(NodeContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        void OnSceneViewResize(RHI::IRHIDevice& device,
                               const ResourceManager& resourceManager) override;

    private:
        std::unique_ptr<ForwardPass> m_Pass;
    };
}
