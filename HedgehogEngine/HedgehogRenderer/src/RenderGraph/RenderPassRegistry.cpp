#include "RenderPassRegistry.hpp"

#include "RenderGraph/IRenderPass.hpp"

#include "Logger/api/Logger.hpp"

namespace Renderer
{
    void RenderPassRegistry::Register(std::string typeId, PassFactory factory)
    {
        m_Factories[std::move(typeId)] = std::move(factory);
    }

    bool RenderPassRegistry::IsRegistered(const std::string& typeId) const
    {
        return m_Factories.find(typeId) != m_Factories.end();
    }

    std::unique_ptr<IRenderPass> RenderPassRegistry::Create(const std::string& typeId,
                                                             const PassInitContext& init,
                                                             const NodeDesc& node) const
    {
        const auto it = m_Factories.find(typeId);
        if (it == m_Factories.end())
        {
            LOGERROR("RenderPassRegistry: unregistered pass type '", typeId, "'.");
            return nullptr;
        }
        return it->second(init, node);
    }
}
