#pragma once

#include "IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class InitPass;

    class InitNode : public IRenderNode
    {
    public:
        InitNode();

        void Execute(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        std::unique_ptr<InitPass> m_Pass;
    };
}
