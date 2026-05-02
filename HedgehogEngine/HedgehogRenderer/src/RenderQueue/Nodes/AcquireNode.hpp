#pragma once

#include "../IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class InitPass;

    class AcquireNode final : public IRenderNode
    {
    public:
        AcquireNode();
        ~AcquireNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        std::unique_ptr<InitPass> m_Pass;
    };

} // namespace Renderer
