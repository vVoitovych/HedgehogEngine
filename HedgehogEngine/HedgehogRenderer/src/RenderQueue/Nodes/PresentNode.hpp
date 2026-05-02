#pragma once

#include "../IRenderNode.hpp"

#include <memory>

namespace Renderer
{
    class PresentPass;

    class PresentNode final : public IRenderNode
    {
    public:
        PresentNode();
        ~PresentNode() override;

        void Render(RenderContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        std::unique_ptr<PresentPass> m_Pass;
    };

} // namespace Renderer
