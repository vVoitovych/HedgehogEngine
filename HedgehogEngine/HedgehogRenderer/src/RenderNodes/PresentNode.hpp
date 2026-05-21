#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class PresentPass;

    class PresentNode : public IRenderNode
    {
    public:
        PresentNode();

        void Execute(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        std::unique_ptr<PresentPass> m_Pass;
    };
}
