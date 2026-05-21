#include "RenderNodeManager.hpp"

#include "RenderNodes/IRenderNode.hpp"
#include "RenderGraph/RenderGraph.hpp"

#include "Logger/api/Logger.hpp"

#include <yaml-cpp/yaml.h>

#include <Windows.h>
#include <cassert>
#include <filesystem>

namespace
{
    std::filesystem::path GetRepoRoot()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        return std::filesystem::path(buffer)
            .parent_path().parent_path().parent_path().parent_path().parent_path();
    }

    std::string ResolveAbsPath(const std::string& repoRelativePath)
    {
        return (GetRepoRoot() / repoRelativePath.substr(1)).lexically_normal().string();
    }
}

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

    void RenderNodeManager::LoadGraphPreset(const std::string& yamlPath, RenderGraph& outGraph)
    {
        const std::string fullPath = ResolveAbsPath(yamlPath);

        YAML::Node root;
        try { root = YAML::LoadFile(fullPath); }
        catch (const YAML::Exception& e)
        {
            LOGERROR("RenderNodeManager: failed to load graph preset '", fullPath, "': ", e.what());
            assert(false && "Graph preset YAML not found or malformed.");
            return;
        }

        const YAML::Node& nodes = root["nodes"];
        assert(nodes && nodes.IsSequence() && "Graph preset must have a 'nodes' sequence.");

        for (const YAML::Node& entry : nodes)
        {
            const std::string type     = entry["type"].as<std::string>();
            const std::string instance = entry["instance"].as<std::string>();
            RenderNodeHandle  handle   = CreateNode(type, instance);
            outGraph.AddNode(GetNode(handle));
        }

        LOGINFO("RenderNodeManager: loaded graph preset '", yamlPath, "'");
    }
}
