#include "TransitionNode.hpp"

#include "../RenderContext.hpp"
#include "../../ResourceManager/ResourceManager.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    TransitionNode::TransitionNode(std::string resourceName,
                                    RHI::ImageLayout fromLayout,
                                    RHI::ImageLayout toLayout)
        : m_Name(std::move(resourceName))
        , m_FromLayout(fromLayout)
        , m_ToLayout(toLayout)
    {}

    void TransitionNode::Render(RenderContext& ctx)
    {
        auto& texture = const_cast<RHI::IRHITexture&>(ctx.m_ResourceManager.get().GetTexture(m_Name));
        ctx.m_Cmd.get().TransitionTexture(texture, m_FromLayout, m_ToLayout);
    }

} // namespace Renderer
