#include "api/EcsSerializer.hpp"
#include "api/ComponentSerializerRegistry.hpp"
#include "api/Prefab/OverrideSet.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace EcsSerialization
{
namespace
{
    // The id written for an entity of the hierarchy: itself in a scene, its local id in a subtree.
    using IdMap = std::function<ECS::Entity(ECS::Entity)>;

    ECS::Entity SameId(ECS::Entity entity) { return entity; }

    // The root of a prefab instance the provider can still read the prefab of, with its document.
    std::shared_ptr<const YAML::Node> FindInstancePrefab(IPrefabProvider* prefabs, const ECS::ECS& ecs, ECS::Entity entity,
                                                         std::string& outPath)
    {
        if (!prefabs)
            return nullptr;
        const std::optional<PrefabLink> link = prefabs->GetLink(ecs, entity);
        if (!link || link->InstanceRoot != entity || link->PrefabPath.empty())
            return nullptr;
        outPath = link->PrefabPath;
        return prefabs->LoadPrefab(link->PrefabPath);
    }

    bool HasPrefabInstances(IPrefabProvider* prefabs, const ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!prefabs)
            return false;
        if (const std::optional<PrefabLink> link = prefabs->GetLink(ecs, entity);
            link && link->InstanceRoot == entity && !link->PrefabPath.empty())
            return true;
        for (ECS::Entity child : ecs.GetComponent<ECS::HierarchyComponent>(entity).Children)
        {
            if (HasPrefabInstances(prefabs, ecs, child))
                return true;
        }
        return false;
    }

    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity,
                         const ComponentSerializerRegistry& registry, const IdMap& toId, IPrefabProvider* prefabs);

    // An instance root: the prefab, the ids by local id, the overrides and the entities added under
    // the instance (a child of the root or of one of its prefab entities that is not one itself).
    void SerializeInstance(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity root, const std::string& prefabPath,
                           const YAML::Node& prefab, const ComponentSerializerRegistry& registry, IPrefabProvider& prefabs)
    {
        std::vector<ECS::Entity> members;
        std::vector<ECS::Entity> added;
        std::vector<ECS::Entity> pending{ root };
        while (!pending.empty())
        {
            const ECS::Entity member = pending.back();
            pending.pop_back();
            const uint32_t local = prefabs.GetLink(ecs, member)->LocalId;
            if (local >= members.size())
                members.resize(local + 1, ECS::INVALID_ENTITY);
            members[local] = member;
            const auto& children = ecs.GetComponent<ECS::HierarchyComponent>(member).Children;
            for (auto it = children.rbegin(); it != children.rend(); ++it)
            {
                const std::optional<PrefabLink> link = prefabs.GetLink(ecs, *it);
                if (link && link->InstanceRoot == root)
                    pending.push_back(*it);
                else
                    added.insert(added.begin(), *it);
            }
        }

        const auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(root);
        out << YAML::BeginMap;
        out << YAML::Key << "Entity"   << YAML::Value << root;
        out << YAML::Key << "Name"     << YAML::Value << hierarchy.Name;
        out << YAML::Key << "Parent"   << YAML::Value << hierarchy.Parent;
        out << YAML::Key << "Prefab"   << YAML::Value << prefabPath;
        out << YAML::Key << "Entities" << YAML::Value << YAML::Flow << members;
        const OverrideSet overrides = DiffInstance(registry, ecs, members, prefab, prefabs.GetLinkComponentKey());
        if (!overrides.empty())
        {
            out << YAML::Key << "Overrides" << YAML::Value;
            WriteOverrides(out, overrides);
        }
        out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (ECS::Entity entity : added)
            SerializeEntity(out, ecs, entity, registry, SameId, &prefabs);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity,
                         const ComponentSerializerRegistry& registry, const IdMap& toId, IPrefabProvider* prefabs)
    {
        std::string prefabPath;
        if (const std::shared_ptr<const YAML::Node> prefab = FindInstancePrefab(prefabs, ecs, entity, prefabPath))
        {
            SerializeInstance(out, ecs, entity, prefabPath, *prefab, registry, *prefabs);
            return;
        }

        const auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(entity);

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << toId(entity);
        out << YAML::Key << "Name"   << YAML::Value << hierarchy.Name;
        out << YAML::Key << "Parent" << YAML::Value << toId(hierarchy.Parent);

        for (const auto& handler : registry.GetHandlers())
        {
            if (handler.HasComponent(ecs, entity))
                handler.Serialize(out, ecs, entity);
        }

        out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (ECS::Entity child : hierarchy.Children)
            SerializeEntity(out, ecs, child, registry, toId, prefabs);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    // A prefab instance read in the first pass: its root and saved ids are created, so every id the
    // document names is taken before the second pass creates any fresh one.
    struct PendingInstance
    {
        YAML::Node               Node;
        ECS::Entity              Root = ECS::INVALID_ENTITY;
        std::vector<ECS::Entity> Reserved; // the saved ids of its other entities, created empty
    };

    struct SceneLoad
    {
        const ComponentSerializerRegistry& Registry;
        std::string                        SourceName;
        std::vector<PendingInstance>       Instances;
    };

    void DeserializeEntity(ECS::ECS& ecs, const YAML::Node& node, SceneLoad& load);

    void ReserveInstance(ECS::ECS& ecs, const YAML::Node& node, SceneLoad& load)
    {
        PendingInstance instance{ node, node["Entity"].as<ECS::Entity>(), {} };
        ecs.CreateEntity(instance.Root);
        if (const YAML::Node saved = node["Entities"])
        {
            for (const YAML::Node& id : saved)
            {
                const ECS::Entity entity = id.as<ECS::Entity>();
                if (entity < ECS::MAX_ENTITIES && !ecs.IsAlive(entity))
                {
                    ecs.CreateEntity(entity);
                    instance.Reserved.push_back(entity);
                }
            }
        }
        load.Instances.push_back(std::move(instance));
        // Added entities keep their ids; they are attached once the prefab's entities exist.
        for (const auto& child : node["Children"])
            DeserializeEntity(ecs, child, load);
    }

    // Second pass: the prefab instantiated over the reserved ids, its overrides applied, and the
    // added entities attached to the entity named as their parent (the root when it is gone).
    void ResolveInstance(ECS::ECS& ecs, const PendingInstance& instance, SceneLoad& load)
    {
        const YAML::Node& node   = instance.Node;
        const std::string path   = node["Prefab"].as<std::string>();
        const std::string name   = node["Name"].as<std::string>();
        const ECS::Entity parent = node["Parent"].as<ECS::Entity>();
        const std::string what   = load.SourceName + ": instance '" + name + "' of " + path;

        IPrefabProvider*                  prefabs = load.Registry.GetPrefabProvider();
        std::shared_ptr<const YAML::Node> prefab  = prefabs ? prefabs->LoadPrefab(path) : nullptr;
        std::vector<ECS::Entity>          members;
        if (prefab)
        {
            std::vector<ECS::Entity> existing{ instance.Root };
            if (const YAML::Node saved = node["Entities"])
            {
                for (size_t local = 1; local < saved.size(); ++local)
                {
                    const ECS::Entity id       = saved[local].as<ECS::Entity>();
                    const bool        reserved = std::find(instance.Reserved.begin(), instance.Reserved.end(), id) !=
                                          instance.Reserved.end();
                    existing.push_back(reserved ? id : ECS::INVALID_ENTITY);
                }
            }
            InstantiateOptions options;
            options.External         = ExternalReferences::Clear;
            options.LocalEntities    = &members;
            options.ExistingEntities = &existing;
            options.AppendToParent   = false;
            if (EcsSerializer::InstantiateSubtree(load.Registry, ecs, *prefab, parent, path, options) == ECS::INVALID_ENTITY)
                members.clear();
        }

        if (members.empty())
        {
            LOGERROR("[Prefab] " + what + " could not be instantiated; it loads as an empty game object.");
            if (!ecs.HasComponent<ECS::HierarchyComponent>(instance.Root))
                ecs.AddComponent(instance.Root, ECS::HierarchyComponent{});
            members = { instance.Root };
        }
        else
        {
            ApplyOverrides(load.Registry, ecs, members, ReadOverrides(node["Overrides"], what), what);
            prefabs->Link(ecs, members, path);
        }
        auto& rootHierarchy  = ecs.GetComponent<ECS::HierarchyComponent>(instance.Root);
        rootHierarchy.Name   = name;
        rootHierarchy.Parent = parent;

        for (const ECS::Entity id : instance.Reserved)
        {
            if (std::find(members.begin(), members.end(), id) == members.end())
                ecs.DestroyEntity(id);
        }
        for (const auto& child : node["Children"])
        {
            const ECS::Entity entity = child["Entity"].as<ECS::Entity>();
            ECS::Entity       owner  = child["Parent"].as<ECS::Entity>();
            if (std::find(members.begin(), members.end(), owner) == members.end())
                owner = instance.Root;
            // An added child that is an instance itself resolves later (it was reserved after this
            // one) and sets its own parent then.
            if (ecs.HasComponent<ECS::HierarchyComponent>(entity))
                ecs.GetComponent<ECS::HierarchyComponent>(entity).Parent = owner;
            ecs.GetComponent<ECS::HierarchyComponent>(owner).Children.push_back(entity);
        }
    }

    void DeserializeEntity(ECS::ECS& ecs, const YAML::Node& node, SceneLoad& load)
    {
        if (node["Prefab"])
        {
            ReserveInstance(ecs, node, load);
            return;
        }
        const ComponentSerializerRegistry& registry = load.Registry;

        ECS::Entity entity = node["Entity"].as<ECS::Entity>();
        ecs.CreateEntity(entity);
        ecs.AddComponent(entity, ECS::HierarchyComponent{});

        for (const auto& handler : registry.GetHandlers())
        {
            const YAML::Node componentNode = node[handler.YamlKey];
            if (componentNode)
                handler.Deserialize(ecs, entity, componentNode);
        }

        auto& hierarchy    = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        hierarchy.Name   = node["Name"].as<std::string>();
        hierarchy.Parent = node["Parent"].as<ECS::Entity>();

        for (const auto& child : node["Children"])
        {
            hierarchy.Children.push_back(child["Entity"].as<ECS::Entity>());
            DeserializeEntity(ecs, child, load);
        }
    }

    // False, logged, for a Version that is not a positive integer or is newer than this build's.
    bool CheckVersion(const YAML::Node& data, const std::string& what)
    {
        int version = 1;
        if (const YAML::Node versionNode = data["Version"])
        {
            if (!YAML::convert<int>::decode(versionNode, version) || version < 1)
            {
                LOGERROR(what + " has a Version that is not a positive integer.");
                return false;
            }
        }
        if (version > EcsSerializer::FORMAT_VERSION)
        {
            LOGERROR(what + " is format version " + std::to_string(version) + ", but this build reads up to version " +
                     std::to_string(EcsSerializer::FORMAT_VERSION) + ".");
            return false;
        }
        return true;
    }

    // The subtree's entities in depth-first order, its root first.
    void CollectSubtree(const ECS::ECS& ecs, ECS::Entity entity, std::vector<ECS::Entity>& out)
    {
        out.push_back(entity);
        for (ECS::Entity child : ecs.GetComponent<ECS::HierarchyComponent>(entity).Children)
            CollectSubtree(ecs, child, out);
    }

    size_t CountEntities(const YAML::Node& node)
    {
        size_t count = 1;
        for (const auto& child : node["Children"])
            count += CountEntities(child);
        return count;
    }

    // Checks a subtree document's local ids before anything is created: each below the entity
    // count and used once. Returns an error, or empty.
    std::string CheckLocalIds(const YAML::Node& node, std::vector<bool>& seen)
    {
        const ECS::Entity local = node["Entity"].as<ECS::Entity>();
        if (local >= seen.size())
            return "local id " + std::to_string(local) + " is not below the entity count " + std::to_string(seen.size());
        if (seen[local])
            return "local id " + std::to_string(local) + " is used twice";
        seen[local] = true;
        for (const auto& child : node["Children"])
        {
            if (std::string error = CheckLocalIds(child, seen); !error.empty())
                return error;
        }
        return {};
    }

    void InstantiateEntity(ECS::ECS& ecs, const YAML::Node& node, ECS::Entity parent,
                           const std::vector<ECS::Entity>& localToNew, const ComponentSerializerRegistry& registry)
    {
        const ECS::Entity entity = localToNew[node["Entity"].as<ECS::Entity>()];
        ecs.AddComponent(entity, ECS::HierarchyComponent{});

        for (const auto& handler : registry.GetHandlers())
        {
            const YAML::Node componentNode = node[handler.YamlKey];
            if (componentNode)
                handler.Deserialize(ecs, entity, componentNode);
        }

        auto& hierarchy  = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        hierarchy.Name   = node["Name"].as<std::string>();
        hierarchy.Parent = parent;

        for (const auto& child : node["Children"])
        {
            const ECS::Entity copy = localToNew[child["Entity"].as<ECS::Entity>()];
            ecs.GetComponent<ECS::HierarchyComponent>(entity).Children.push_back(copy);
            InstantiateEntity(ecs, child, entity, localToNew, registry);
        }
    }
}

    std::string EcsSerializer::SerializeToString(const ComponentSerializerRegistry& registry,
                                                  const ECS::ECS& ecs,
                                                  const std::string& sceneName)
    {
        IPrefabProvider* prefabs = registry.GetPrefabProvider();
        const int        version = HasPrefabInstances(prefabs, ecs, ecs.GetRoot()) ? PREFAB_INSTANCES_FORMAT_VERSION
                                                                                   : BASE_FORMAT_VERSION;
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version"    << YAML::Value << version;
        out << YAML::Key << "Scene name" << YAML::Value << sceneName;
        out << YAML::Key << "Scene"      << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, ecs, ecs.GetRoot(), registry, SameId, prefabs);
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return out.c_str();
    }

    bool EcsSerializer::DeserializeFromString(const ComponentSerializerRegistry& registry,
                                              ECS::ECS& ecs,
                                              std::string& outSceneName,
                                              const std::string& yamlText,
                                              const std::string& sourceName)
    {
        YAML::Node document;
        try
        {
            document = YAML::Load(yamlText);
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("Failed to parse scene: ", sourceName, " with error: ", e.what());
            return false;
        }
        return DeserializeFromNode(registry, ecs, outSceneName, document, sourceName);
    }

    YAML::Node EcsSerializer::SerializeToNode(const ComponentSerializerRegistry& registry,
                                              const ECS::ECS& ecs,
                                              const std::string& sceneName)
    {
        // Components write themselves to an emitter, so the text is the one source of truth.
        return YAML::Load(SerializeToString(registry, ecs, sceneName));
    }

    bool EcsSerializer::DeserializeFromNode(const ComponentSerializerRegistry& registry,
                                            ECS::ECS& ecs,
                                            std::string& outSceneName,
                                            const YAML::Node& data,
                                            const std::string& sourceName)
    {
        try
        {
            if (!CheckVersion(data, "Failed to read scene: " + sourceName))
                return false;

            outSceneName = data["Scene name"].as<std::string>();

            const YAML::Node sceneData = data["Scene"];
            if (sceneData && sceneData.size() > 0)
            {
                SceneLoad load{ registry, sourceName, {} };
                for (const auto& node : sceneData)
                    DeserializeEntity(ecs, node, load);
                for (const PendingInstance& instance : load.Instances)
                    ResolveInstance(ecs, instance, load);

                // The first node in the scene sequence is the root entity.
                ecs.SetRoot(sceneData[0]["Entity"].as<ECS::Entity>());
            }
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR("Failed to parse scene: ", sourceName, " with error: ", e.what());
            return false;
        }
        return true;
    }

    bool EcsSerializer::Serialize(const ComponentSerializerRegistry& registry,
                                   const ECS::ECS& ecs,
                                   const std::string& sceneName,
                                   const std::string& virtualPath,
                                   const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("EcsSerializer::Serialize: ", virtualPath);

        if (!fileSystem.WriteTextFile(virtualPath, SerializeToString(registry, ecs, sceneName)))
        {
            LOGERROR("EcsSerializer::Serialize: failed to write '", virtualPath, "'.");
            return false;
        }
        return true;
    }

    bool EcsSerializer::Deserialize(const ComponentSerializerRegistry& registry,
                                     ECS::ECS& ecs,
                                     std::string& outSceneName,
                                     const std::string& virtualPath,
                                     const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("EcsSerializer::Deserialize: ", virtualPath);

        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("Failed to read scene file: ", virtualPath);
            return false;
        }

        return DeserializeFromString(registry, ecs, outSceneName, *text, virtualPath);
    }

    std::string EcsSerializer::SerializeSubtreeToString(const ComponentSerializerRegistry& registry,
                                                         const ECS::ECS& ecs,
                                                         ECS::Entity subtreeRoot)
    {
        std::vector<ECS::Entity> sourceIds;
        CollectSubtree(ecs, subtreeRoot, sourceIds);
        std::unordered_map<ECS::Entity, ECS::Entity> localIds;
        for (size_t i = 0; i < sourceIds.size(); ++i)
            localIds.emplace(sourceIds[i], i);
        // The root's parent lies outside, so it is written as INVALID_ENTITY, as a scene root's is.
        const IdMap toLocal = [&localIds](ECS::Entity entity)
        {
            const auto it = localIds.find(entity);
            return it == localIds.end() ? ECS::INVALID_ENTITY : it->second;
        };

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version"   << YAML::Value << BASE_FORMAT_VERSION;
        out << YAML::Key << "SourceIds" << YAML::Value << YAML::Flow << sourceIds;
        out << YAML::Key << "Subtree"   << YAML::Value << YAML::BeginSeq;
        // Written in full: a prefab inside a prefab is not kept as a reference yet.
        SerializeEntity(out, ecs, subtreeRoot, registry, toLocal, nullptr);
        out << YAML::EndSeq;
        out << YAML::EndMap;
        return out.c_str();
    }

    YAML::Node EcsSerializer::SerializeSubtree(const ComponentSerializerRegistry& registry,
                                               const ECS::ECS& ecs,
                                               ECS::Entity subtreeRoot)
    {
        return YAML::Load(SerializeSubtreeToString(registry, ecs, subtreeRoot));
    }

    ECS::Entity EcsSerializer::InstantiateSubtree(const ComponentSerializerRegistry& registry,
                                                  ECS::ECS& ecs,
                                                  const YAML::Node& document,
                                                  ECS::Entity parent,
                                                  const std::string& sourceName,
                                                  const InstantiateOptions& options)
    {
        const std::string failure = "Failed to instantiate " + sourceName;
        if (!ecs.IsAlive(parent) || !ecs.HasComponent<ECS::HierarchyComponent>(parent))
        {
            LOGERROR(failure + ": parent entity " + std::to_string(parent) + " has no hierarchy.");
            return ECS::INVALID_ENTITY;
        }

        std::vector<ECS::Entity> localToNew;
        std::vector<ECS::Entity> created; // destroyed again on failure; existing ones are the caller's
        try
        {
            if (!CheckVersion(document, failure))
                return ECS::INVALID_ENTITY;

            const YAML::Node subtree = document["Subtree"];
            if (!subtree || !subtree.IsSequence() || subtree.size() != 1)
            {
                LOGERROR(failure + ": Subtree is not a sequence of one entity.");
                return ECS::INVALID_ENTITY;
            }
            const auto   sourceIds = document["SourceIds"].as<std::vector<ECS::Entity>>();
            const size_t count     = CountEntities(subtree[0]);
            if (count != sourceIds.size())
            {
                LOGERROR(failure + ": SourceIds lists " + std::to_string(sourceIds.size()) + " entities, but Subtree holds " +
                         std::to_string(count) + ".");
                return ECS::INVALID_ENTITY;
            }
            std::vector<bool> seen(count, false);
            if (const std::string error = CheckLocalIds(subtree[0], seen); !error.empty())
            {
                LOGERROR(failure + ": " + error + ".");
                return ECS::INVALID_ENTITY;
            }
            const auto existingFor = [&options](size_t local)
            {
                const std::vector<ECS::Entity>* existing = options.ExistingEntities;
                return existing && local < existing->size() ? (*existing)[local] : ECS::INVALID_ENTITY;
            };
            size_t fresh = 0;
            for (size_t i = 0; i < count; ++i)
                fresh += existingFor(i) == ECS::INVALID_ENTITY ? 1 : 0;
            if (ecs.GetEntityCount() + fresh > ECS::MAX_ENTITIES)
            {
                LOGERROR(failure + ": its " + std::to_string(count) + " entities do not fit beside the " +
                         std::to_string(ecs.GetEntityCount()) + " alive (at most " + std::to_string(ECS::MAX_ENTITIES) + ").");
                return ECS::INVALID_ENTITY;
            }

            for (size_t i = 0; i < count; ++i)
            {
                const ECS::Entity existing = existingFor(i);
                localToNew.push_back(existing != ECS::INVALID_ENTITY ? existing : ecs.CreateEntity());
                if (existing == ECS::INVALID_ENTITY)
                    created.push_back(localToNew.back());
            }
            InstantiateEntity(ecs, subtree[0], parent, localToNew, registry);

            std::unordered_map<ECS::Entity, ECS::Entity> sourceToNew;
            for (size_t i = 0; i < count; ++i)
                sourceToNew.emplace(sourceIds[i], localToNew[i]);
            const EntityRemap remap = [&](ECS::Entity entity)
            {
                if (const auto it = sourceToNew.find(entity); it != sourceToNew.end())
                    return it->second;
                // One outside the subtree is kept while alive. A dead one's id may have been
                // reused by a copy, which it never named.
                const bool isCopy = std::find(localToNew.begin(), localToNew.end(), entity) != localToNew.end();
                const bool keep   = options.External == ExternalReferences::Keep && ecs.IsAlive(entity) && !isCopy;
                return keep ? entity : ECS::INVALID_ENTITY;
            };
            for (ECS::Entity entity : localToNew)
            {
                for (const auto& handler : registry.GetHandlers())
                {
                    if (handler.RemapEntities && handler.HasComponent(ecs, entity))
                        handler.RemapEntities(ecs, entity, remap);
                }
            }
        }
        catch (const YAML::Exception& e)
        {
            LOGERROR(failure + ": " + e.what());
            for (ECS::Entity entity : created)
                ecs.DestroyEntity(entity);
            return ECS::INVALID_ENTITY;
        }

        if (options.AppendToParent)
            ecs.GetComponent<ECS::HierarchyComponent>(parent).Children.push_back(localToNew[0]);
        if (options.LocalEntities)
            *options.LocalEntities = localToNew;
        return localToNew[0];
    }
}
