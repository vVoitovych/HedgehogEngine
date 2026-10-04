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
                                 EcsSerialization::ComponentSerializerRegistry& registry, SceneManager& sceneManager)
        : m_ECS(ecs)
        , m_FileSystem(fileSystem)
        , m_Registry(registry)
        , m_SceneManager(sceneManager)
    {
        m_Registry.SetPrefabProvider(this);
    }

    PrefabManager::~PrefabManager()
    {
        if (m_Registry.GetPrefabProvider() == this)
            m_Registry.SetPrefabProvider(nullptr);
    }

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

    ECS::Entity PrefabManager::GetInstanceRoot(ECS::Entity entity) const
    {
        if (!m_ECS.IsAlive(entity) || !m_ECS.HasComponent<PrefabInstanceComponent>(entity))
            return ECS::INVALID_ENTITY;
        const ECS::Entity root = m_ECS.GetComponent<PrefabInstanceComponent>(entity).InstanceRoot;
        const bool        isRoot = m_ECS.IsAlive(root) && m_ECS.HasComponent<PrefabInstanceComponent>(root) &&
                            m_ECS.GetComponent<PrefabInstanceComponent>(root).InstanceRoot == root &&
                            !m_ECS.GetComponent<PrefabInstanceComponent>(root).PrefabPath.empty();
        return isRoot ? root : ECS::INVALID_ENTITY;
    }

    EcsSerialization::OverrideSet PrefabManager::GetOverrides(ECS::Entity entity)
    {
        const ECS::Entity root = GetInstanceRoot(entity);
        if (root == ECS::INVALID_ENTITY)
            return {};
        const std::shared_ptr<const YAML::Node> prefab = Load(m_ECS.GetComponent<PrefabInstanceComponent>(root).PrefabPath);
        if (!prefab)
            return {};
        std::vector<ECS::Entity> members;
        EcsSerialization::CollectInstance(*this, m_ECS, root, members);
        return EcsSerialization::DiffInstance(m_Registry, m_ECS, members, *prefab, GetLinkComponentKey());
    }

    void PrefabManager::SetValue(ECS::Entity entity, const EcsSerialization::PropertyOverride& value)
    {
        const EcsSerialization::ComponentHandler* handler = m_Registry.FindHandler(value.Component);
        if (!handler)
            return;
        if (value.Property.empty())
        {
            handler->Deserialize(m_ECS, entity, value.Value);
            return;
        }
        YAML::Node partial;
        partial[value.Property] = YAML::Clone(value.Value);
        handler->Deserialize(m_ECS, entity, partial);
    }

    bool PrefabManager::Revert(ECS::Entity entity, const EcsSerialization::OverrideSet& overrides)
    {
        const ECS::Entity root = GetInstanceRoot(entity);
        if (root == ECS::INVALID_ENTITY)
        {
            LOGERROR("[Prefab] Entity " + std::to_string(entity) + " is not part of a prefab instance.");
            return false;
        }
        const std::string                       path   = m_ECS.GetComponent<PrefabInstanceComponent>(root).PrefabPath;
        const std::shared_ptr<const YAML::Node> prefab = Load(path);
        if (!prefab)
            return false;
        std::vector<ECS::Entity> members;
        EcsSerialization::CollectInstance(*this, m_ECS, root, members);
        const std::vector<YAML::Node> nodes = EcsSerialization::IndexSubtree(*prefab);

        for (const EcsSerialization::PropertyOverride& entry : overrides)
        {
            if (entry.LocalId >= members.size() || members[entry.LocalId] == ECS::INVALID_ENTITY ||
                entry.LocalId >= nodes.size())
                continue;
            const ECS::Entity                          target    = members[entry.LocalId];
            const EcsSerialization::ComponentHandler*  handler   = m_Registry.FindHandler(entry.Component);
            const YAML::Node                           component = nodes[entry.LocalId][entry.Component];
            if (!handler)
                continue;
            if (!component)
            {
                // A component the instance added: reverting removes it.
                if (handler->Remove && handler->HasComponent(m_ECS, target))
                    handler->Remove(m_ECS, target);
                continue;
            }
            if (entry.Property.empty())
                SetValue(target, { entry.LocalId, entry.Component, {}, component });
            else if (const YAML::Node value = component[entry.Property])
                SetValue(target, { entry.LocalId, entry.Component, entry.Property, value });
        }
        m_SceneManager.RefreshAfterLoad();
        return true;
    }

    std::vector<ECS::Entity> PrefabManager::FindInstances(const std::string& path) const
    {
        std::vector<ECS::Entity> roots;
        std::vector<ECS::Entity> pending{ m_ECS.GetRoot() };
        while (!pending.empty())
        {
            const ECS::Entity entity = pending.back();
            pending.pop_back();
            if (m_ECS.HasComponent<PrefabInstanceComponent>(entity))
            {
                const auto& link = m_ECS.GetComponent<PrefabInstanceComponent>(entity);
                if (link.InstanceRoot == entity && link.PrefabPath == path)
                    roots.push_back(entity);
            }
            const auto& children = m_ECS.GetComponent<ECS::HierarchyComponent>(entity).Children;
            pending.insert(pending.end(), children.begin(), children.end());
        }
        return roots;
    }

    bool PrefabManager::Apply(ECS::Entity entity, const EcsSerialization::OverrideSet& overrides)
    {
        const ECS::Entity root = GetInstanceRoot(entity);
        if (root == ECS::INVALID_ENTITY)
        {
            LOGERROR("[Prefab] Entity " + std::to_string(entity) + " is not part of a prefab instance.");
            return false;
        }
        const std::string                       path   = m_ECS.GetComponent<PrefabInstanceComponent>(root).PrefabPath;
        const std::shared_ptr<const YAML::Node> prefab = Load(path);
        if (!prefab)
            return false;

        // What every other instance overrides now, before the prefab changes under it.
        struct OtherInstance
        {
            std::vector<ECS::Entity>      Members;
            EcsSerialization::OverrideSet Overrides;
        };
        std::vector<OtherInstance> others;
        for (const ECS::Entity other : FindInstances(path))
        {
            if (other == root)
                continue;
            OtherInstance instance;
            EcsSerialization::CollectInstance(*this, m_ECS, other, instance.Members);
            instance.Overrides = EcsSerialization::DiffInstance(m_Registry, m_ECS, instance.Members, *prefab, GetLinkComponentKey());
            others.push_back(std::move(instance));
        }

        std::vector<ECS::Entity> members;
        EcsSerialization::CollectInstance(*this, m_ECS, root, members);
        const EcsSerialization::EntityRemap toPrefab = EcsSerialization::MakeInstanceToPrefabRemap(*prefab, members);

        YAML::Node                    document = YAML::Clone(*prefab);
        const std::vector<YAML::Node> nodes    = EcsSerialization::IndexSubtree(document);
        EcsSerialization::OverrideSet written; // the values as the prefab now holds them
        for (const EcsSerialization::PropertyOverride& entry : overrides)
        {
            const EcsSerialization::ComponentHandler* handler = m_Registry.FindHandler(entry.Component);
            if (entry.LocalId >= nodes.size() || !nodes[entry.LocalId] || !handler)
                continue;
            // The value as the prefab names entities: the instance's own as their source ids.
            YAML::Node value;
            if (entry.Property.empty())
                value = YAML::Clone(entry.Value);
            else
                value[entry.Property] = YAML::Clone(entry.Value);
            if (handler->RemapYaml)
                handler->RemapYaml(value, toPrefab);

            YAML::Node node = nodes[entry.LocalId];
            if (entry.Property.empty())
                node[entry.Component] = value;
            else
                node[entry.Component][entry.Property] = value[entry.Property];
            written.push_back({ entry.LocalId, entry.Component, entry.Property, value });
        }
        if (!m_FileSystem.WriteTextFile(path, EcsSerialization::WritePrefabDocument(document)))
        {
            LOGERROR("[Prefab] " + path + " could not be written.");
            return false;
        }
        m_Cache.erase(path);

        for (const OtherInstance& instance : others)
        {
            const EcsSerialization::EntityRemap toInstance = EcsSerialization::MakePrefabToInstanceRemap(document, instance.Members);
            for (const EcsSerialization::PropertyOverride& entry : written)
            {
                if (entry.LocalId >= instance.Members.size() || instance.Members[entry.LocalId] == ECS::INVALID_ENTITY)
                    continue;
                const bool overridden = std::any_of(instance.Overrides.begin(), instance.Overrides.end(),
                                                    [&entry](const EcsSerialization::PropertyOverride& own)
                                                    {
                                                        return own.LocalId == entry.LocalId && own.Component == entry.Component &&
                                                               (own.Property == entry.Property || own.Property.empty());
                                                    });
                if (overridden)
                    continue;
                // The prefab's value with its entity ids as this instance's.
                YAML::Node value = YAML::Clone(entry.Value);
                if (const auto* handler = m_Registry.FindHandler(entry.Component); handler && handler->RemapYaml)
                    handler->RemapYaml(value, toInstance);
                SetValue(instance.Members[entry.LocalId],
                         { entry.LocalId, entry.Component, entry.Property, entry.Property.empty() ? value : value[entry.Property] });
            }
        }
        m_SceneManager.RefreshAfterLoad();
        LOGINFO("[Prefab] Applied " + std::to_string(overrides.size()) + " overrides to " + path + ".");
        return true;
    }

    std::optional<EcsSerialization::PrefabLink> PrefabManager::GetLink(const ECS::ECS& ecs, ECS::Entity entity) const
    {
        if (!ecs.HasComponent<PrefabInstanceComponent>(entity))
            return std::nullopt;
        const auto& link = ecs.GetComponent<PrefabInstanceComponent>(entity);
        return EcsSerialization::PrefabLink{ link.PrefabPath, link.LocalId, link.InstanceRoot };
    }

    const char* PrefabManager::GetLinkComponentKey() const { return PrefabInstanceComponent::s_TypeName; }

    std::shared_ptr<const YAML::Node> PrefabManager::LoadPrefab(const std::string& virtualPath)
    {
        const std::string path = NormalizePrefabPath(virtualPath);
        if (path.empty())
        {
            LOGERROR("[Prefab] '" + virtualPath + "' is not a .prefab path.");
            return nullptr;
        }
        return Load(path);
    }

    void PrefabManager::Link(ECS::ECS& ecs, const std::vector<ECS::Entity>& entities, const std::string& path)
    {
        for (size_t localId = 0; localId < entities.size(); ++localId)
        {
            if (entities[localId] == ECS::INVALID_ENTITY)
                continue;
            PrefabInstanceComponent link;
            link.PrefabPath   = localId == 0 ? NormalizePrefabPath(path) : std::string{};
            link.LocalId      = static_cast<uint32_t>(localId);
            link.InstanceRoot = entities[0];
            if (ecs.HasComponent<PrefabInstanceComponent>(entities[localId]))
                ecs.GetComponent<PrefabInstanceComponent>(entities[localId]) = link;
            else
                ecs.AddComponent(entities[localId], link);
        }
    }
}
