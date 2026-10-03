#include "api/EcsSerializer.hpp"
#include "api/ComponentSerializerRegistry.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <stdexcept>
#include <string>

namespace EcsSerialization
{
namespace
{
    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity,
                         const ComponentSerializerRegistry& registry)
    {
        const auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(entity);

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity;
        out << YAML::Key << "Name"   << YAML::Value << hierarchy.Name;
        out << YAML::Key << "Parent" << YAML::Value << hierarchy.Parent;

        for (const auto& handler : registry.GetHandlers())
        {
            if (handler.HasComponent(ecs, entity))
                handler.Serialize(out, ecs, entity);
        }

        out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (ECS::Entity child : hierarchy.Children)
            SerializeEntity(out, ecs, child, registry);
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
        SerializeEntity(out, ecs, ecs.GetRoot(), registry);
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
            int version = 1;
            if (const YAML::Node versionNode = data["Version"])
            {
                if (!YAML::convert<int>::decode(versionNode, version) || version < 1)
                {
                    LOGERROR("Failed to read scene: " + sourceName + " has a Version that is not a positive integer.");
                    return false;
                }
            }
            if (version > FORMAT_VERSION)
            {
                LOGERROR("Failed to read scene: " + sourceName + " is format version " + std::to_string(version) +
                         ", but this build reads up to version " + std::to_string(FORMAT_VERSION) + ".");
                return false;
            }

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
}
