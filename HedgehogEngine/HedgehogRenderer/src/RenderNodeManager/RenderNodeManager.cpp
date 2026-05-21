#include "RenderNodeManager.hpp"

#include "RenderNodes/IRenderNode.hpp"
#include "RenderGraph/RenderGraph.hpp"

#include <cassert>

namespace Renderer
{
    void RenderNodeManager::RegisterNodeType(
        const std::string& typeId,
        std::function<std::unique_ptr<IRenderNode>()> factory)
    {
        assert(m_Factories.find(typeId) == m_Factories.end()
            && "RenderNodeManager: node type already registered");
        m_Factories[typeId] = std::move(factory);
    }

    RenderNodeHandle RenderNodeManager::CreateNode(
        const std::string& typeId, const std::string& instanceName)
    {
        auto factoryIt = m_Factories.find(typeId);
        assert(factoryIt != m_Factories.end()
            && "RenderNodeManager: unknown node type");

        RenderNodeHandle handle = m_NextHandle++;
        m_Instances[handle]     = factoryIt->second();
        m_ByName[instanceName]  = handle;
        return handle;
    }

    void RenderNodeManager::DestroyNode(RenderNodeHandle handle)
    {
        auto instIt = m_Instances.find(handle);
        if (instIt == m_Instances.end())
            return;

        for (auto nameIt = m_ByName.begin(); nameIt != m_ByName.end(); ++nameIt)
        {
            if (nameIt->second == handle)
            {
                m_ByName.erase(nameIt);
                break;
            }
        }
        m_Instances.erase(instIt);
    }

    void RenderNodeManager::DestroyAll()
    {
        m_Instances.clear();
        m_ByName.clear();
    }

    IRenderNode* RenderNodeManager::GetNode(RenderNodeHandle handle) const
    {
        auto it = m_Instances.find(handle);
        return it != m_Instances.end() ? it->second.get() : nullptr;
    }

    IRenderNode* RenderNodeManager::FindNode(const std::string& instanceName) const
    {
        auto nameIt = m_ByName.find(instanceName);
        if (nameIt == m_ByName.end())
            return nullptr;
        return GetNode(nameIt->second);
    }

    void RenderNodeManager::SetNodeEnabled(RenderNodeHandle handle, bool enabled)
    {
        if (IRenderNode* node = GetNode(handle))
            node->SetEnabled(enabled);
    }

    void RenderNodeManager::LoadGraphPreset(const GraphPreset& preset, RenderGraph& outGraph)
    {
        for (const auto& entry : preset.nodes)
        {
            RenderNodeHandle handle = CreateNode(entry.type, entry.instance);
            outGraph.AddNode(GetNode(handle));
        }
    }
}
