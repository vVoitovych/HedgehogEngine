#pragma once

#include "../IRenderNode.hpp"

#include "RHI/api/RHITypes.hpp"

#include <string>

namespace Renderer
{
    class TransitionNode final : public IRenderNode
    {
    public:
        TransitionNode(std::string resourceName,
                       RHI::ImageLayout fromLayout,
                       RHI::ImageLayout toLayout);

        void Render(RenderContext& ctx) override;

    private:
        std::string      m_Name;
        RHI::ImageLayout m_FromLayout;
        RHI::ImageLayout m_ToLayout;
    };

} // namespace Renderer
