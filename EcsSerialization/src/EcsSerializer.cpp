#include "api/EcsSerializer.hpp"
#include "api/ComponentSerializerRegistry.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <functional>
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

    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity,
                         const ComponentSerializerRegistry& registry, const IdMap& toId)
    {
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
            SerializeEntity(out, ecs, child, registry, toId);
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    void DeserializeEntity(ECS::ECS& ecs, const YAML::Node& node,
                           const ComponentSerializerRegistry& registry)
    {
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
            DeserializeEntity(ecs, child, registry);
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
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version"    << YAML::Value << FORMAT_VERSION;
        out << YAML::Key << "Scene name" << YAML::Value << sceneName;
        out << YAML::Key << "Scene"      << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, ecs, ecs.GetRoot(), registry, SameId);
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
                for (const auto& node : sceneData)
                    DeserializeEntity(ecs, node, registry);

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
        out << YAML::Key << "Version"   << YAML::Value << FORMAT_VERSION;
        out << YAML::Key << "SourceIds" << YAML::Value << YAML::Flow << sourceIds;
        out << YAML::Key << "Subtree"   << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, ecs, subtreeRoot, registry, toLocal);
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
            if (ecs.GetEntityCount() + count > ECS::MAX_ENTITIES)
            {
                LOGERROR(failure + ": its " + std::to_string(count) + " entities do not fit beside the " +
                         std::to_string(ecs.GetEntityCount()) + " alive (at most " + std::to_string(ECS::MAX_ENTITIES) + ").");
                return ECS::INVALID_ENTITY;
            }

            for (size_t i = 0; i < count; ++i)
                localToNew.push_back(ecs.CreateEntity());
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
            for (ECS::Entity entity : localToNew)
                ecs.DestroyEntity(entity);
            return ECS::INVALID_ENTITY;
        }

        ecs.GetComponent<ECS::HierarchyComponent>(parent).Children.push_back(localToNew[0]);
        if (options.LocalEntities)
            *options.LocalEntities = localToNew;
        return localToNew[0];
    }
}
