#include "api/EcsSerializer.hpp"
#include "api/ComponentSerializerRegistry.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <stdexcept>

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
                                               const std::string& text)
    {
        try
        {
            YAML::Node data = YAML::Load(text);

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
            LOGERROR("Failed to parse scene from string with error: ", e.what());
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

        const std::string text = SerializeToString(registry, ecs, sceneName);
        if (!fileSystem.WriteTextFile(virtualPath, text))
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
        return DeserializeFromString(registry, ecs, outSceneName, *text);
    }
}
