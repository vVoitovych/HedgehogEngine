#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"

#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/Prefab/PrefabDocument.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <system_error>
#include <vector>

namespace HedgehogEngine
{
    namespace
    {
        bool EndsWithIgnoringCase(const std::string& text, std::string_view suffix)
        {
            if (text.size() < suffix.size())
                return false;
            return std::equal(suffix.begin(), suffix.end(), text.end() - static_cast<std::ptrdiff_t>(suffix.size()),
                              [](char a, char b)
                              {
                                  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                              });
        }
    }

    PrefabManager::PrefabManager(ECS::ECS& ecs, const FS::FileSystemManager& fileSystem,
                                 const EcsSerialization::ComponentSerializerRegistry& registry, SceneManager& sceneManager)
        : m_ECS(ecs)
        , m_FileSystem(fileSystem)
        , m_Registry(registry)
        , m_SceneManager(sceneManager)
    {
    }

    PrefabManager::~PrefabManager() = default;

    std::string PrefabManager::NormalizePrefabPath(const std::string& path)
    {
        const std::string normalized = EcsSerialization::NormalizeAssetPath(path);
        return EndsWithIgnoringCase(normalized, EcsSerialization::PREFAB_EXTENSION) ? normalized : std::string{};
    }

    bool PrefabManager::CreatePrefab(ECS::Entity entity, const std::string& virtualPath)
    {
        const std::string path = NormalizePrefabPath(virtualPath);
        if (path.empty())
        {
            LOGERROR("[Prefab] '" + virtualPath + "' is not a .prefab path.");
            return false;
        }
        if (entity == m_ECS.GetRoot() || !m_ECS.IsAlive(entity) || !m_ECS.HasComponent<ECS::HierarchyComponent>(entity))
        {
            LOGERROR("[Prefab] " + path + ": entity " + std::to_string(entity) + " is the scene root or not a game object.");
            return false;
        }
        if (!m_FileSystem.WriteTextFile(path, EcsSerialization::WritePrefab(m_Registry, m_ECS, entity)))
        {
            LOGERROR("[Prefab] " + path + " could not be written.");
            return false;
        }
        m_Cache.erase(path);
        LOGINFO("[Prefab] Created " + path + ".");
        return true;
    }

    std::shared_ptr<const YAML::Node> PrefabManager::Load(const std::string& path)
    {
        const std::optional<std::filesystem::path> physical = m_FileSystem.ResolvePhysical(path);
        std::error_code                            error;
        const auto writeTime = physical ? std::filesystem::last_write_time(*physical, error) : std::filesystem::file_time_type{};
        if (!physical || error)
        {
            LOGERROR("[Prefab] " + path + " does not exist.");
            return nullptr;
        }

        if (const auto cached = m_Cache.find(path); cached != m_Cache.end() && cached->second.WriteTime == writeTime)
            return cached->second.Root;

        const std::optional<std::string> text = m_FileSystem.ReadTextFile(path);
        if (!text)
        {
            LOGERROR("[Prefab] " + path + " cannot be read.");
            return nullptr;
        }
        EcsSerialization::PrefabReadResult read = EcsSerialization::ReadPrefab(*text);
        if (!read.Root)
        {
            LOGERROR("[Prefab] " + path + ": " + read.Error + ".");
            return nullptr;
        }
        auto root     = std::make_shared<const YAML::Node>(std::move(*read.Root));
        m_Cache[path] = CachedPrefab{ root, writeTime };
        return root;
    }

    ECS::Entity PrefabManager::Instantiate(const std::string& virtualPath, std::optional<ECS::Entity> parent)
    {
        const std::string path = NormalizePrefabPath(virtualPath);
        if (path.empty())
        {
            LOGERROR("[Prefab] '" + virtualPath + "' is not a .prefab path.");
            return ECS::INVALID_ENTITY;
        }
        const std::shared_ptr<const YAML::Node> root = Load(path);
        if (!root)
            return ECS::INVALID_ENTITY;

        EcsSerialization::InstantiateOptions options;
        options.External = EcsSerialization::ExternalReferences::Clear;
        const ECS::Entity instance = EcsSerialization::EcsSerializer::InstantiateSubtree(
            m_Registry, m_ECS, *root, parent.value_or(m_ECS.GetRoot()), path, options);
        if (instance == ECS::INVALID_ENTITY)
            return ECS::INVALID_ENTITY; // logged, naming the path

        // The copy keeps the document's depth-first order, so its walk gives the local ids.
        LinkSubtree(instance, path);
        m_SceneManager.RefreshAfterLoad();
        return instance;
    }

    bool PrefabManager::LinkInstance(ECS::Entity entity, const std::string& virtualPath)
    {
        const std::string path = NormalizePrefabPath(virtualPath);
        if (path.empty())
        {
            LOGERROR("[Prefab] '" + virtualPath + "' is not a .prefab path.");
            return false;
        }
        if (entity == m_ECS.GetRoot() || !m_ECS.IsAlive(entity) || !m_ECS.HasComponent<ECS::HierarchyComponent>(entity))
        {
            LOGERROR("[Prefab] " + path + ": entity " + std::to_string(entity) + " is the scene root or not a game object.");
            return false;
        }
        LinkSubtree(entity, path);
        return true;
    }

    void PrefabManager::LinkSubtree(ECS::Entity instanceRoot, const std::string& path)
    {
        uint32_t                 localId = 0;
        std::vector<ECS::Entity> pending{ instanceRoot };
        while (!pending.empty())
        {
            const ECS::Entity current = pending.back();
            pending.pop_back();

            PrefabInstanceComponent link;
            link.PrefabPath   = current == instanceRoot ? path : std::string{};
            link.LocalId      = localId++;
            link.InstanceRoot = instanceRoot;
            // An entity linked before (a prefab made from an instance) takes the new link.
            if (m_ECS.HasComponent<PrefabInstanceComponent>(current))
                m_ECS.GetComponent<PrefabInstanceComponent>(current) = link;
            else
                m_ECS.AddComponent(current, link);

            const auto& children = m_ECS.GetComponent<ECS::HierarchyComponent>(current).Children;
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
    }

    size_t PrefabManager::GetCachedPrefabCount() const { return m_Cache.size(); }
}
