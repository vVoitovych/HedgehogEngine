#include "TransitionNode.hpp"

#include "../RenderContext.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    TransitionNode::TransitionNode(ResourceAccessor accessor,
                                    RHI::ImageLayout fromLayout,
                                    RHI::ImageLayout toLayout)
        : m_Accessor(std::move(accessor))
        , m_FromLayout(fromLayout)
        , m_ToLayout(toLayout)
    {}

    void TransitionNode::Render(RenderContext& ctx)
    {
        RHI::IRHITexture& texture = m_Accessor(*ctx.m_ResourceManager);
        ctx.m_Cmd->TransitionTexture(texture, m_FromLayout, m_ToLayout);
    }

} // namespace Renderer
