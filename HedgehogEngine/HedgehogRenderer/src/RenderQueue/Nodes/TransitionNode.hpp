#pragma once

#include "../IRenderNode.hpp"

#include "RHI/api/RHITypes.hpp"

#include <functional>

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    // Inserts a single pipeline barrier that transitions one texture between layouts.
    // The texture is resolved at construction time via a ResourceAccessor callback,
    // keeping the node decoupled from the specific ResourceManager getter.
    class TransitionNode final : public IRenderNode
    {
    public:
        // Returns a mutable reference to the target texture.
        // Caller is responsible for the const_cast where needed.
        using ResourceAccessor = std::function<RHI::IRHITexture&(const ResourceManager&)>;

        TransitionNode(ResourceAccessor accessor,
                       RHI::ImageLayout fromLayout,
                       RHI::ImageLayout toLayout);

        void Render(RenderContext& ctx) override;

    private:
        ResourceAccessor m_Accessor;
        RHI::ImageLayout m_FromLayout;
        RHI::ImageLayout m_ToLayout;
    };

} // namespace Renderer
