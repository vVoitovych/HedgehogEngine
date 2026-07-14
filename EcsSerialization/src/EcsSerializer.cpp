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
        out << YAML::Key << "Name"   << YAML::Value << hierarchy.m_Name;
        out << YAML::Key << "Parent" << YAML::Value << hierarchy.m_Parent;

        for (const auto& handler : registry.GetHandlers())
        {
            if (handler.m_HasComponent(ecs, entity))
                handler.m_Serialize(out, ecs, entity);
        }

        out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (ECS::Entity child : hierarchy.m_Children)
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
            const YAML::Node componentNode = node[handler.m_YamlKey];
            if (componentNode)
                handler.m_Deserialize(ecs, entity, componentNode);
        }

        auto& hierarchy    = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        hierarchy.m_Name   = node["Name"].as<std::string>();
        hierarchy.m_Parent = node["Parent"].as<ECS::Entity>();

        for (const auto& child : node["Children"])
        {
            hierarchy.m_Children.push_back(child["Entity"].as<ECS::Entity>());
            DeserializeEntity(ecs, child, registry);
        }
    }
}

    void EcsSerializer::Serialize(const ComponentSerializerRegistry& registry,
                                   const ECS::ECS& ecs,
                                   const std::string& sceneName,
                                   const std::string& virtualPath,
                                   const FS::FileSystemManager& fileSystem)
    {
        LOGINFO("EcsSerializer::Serialize: ", virtualPath);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene name" << YAML::Value << sceneName;
        out << YAML::Key << "Scene"      << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, ecs, ecs.GetRoot(), registry);
        out << YAML::EndSeq;
        out << YAML::EndMap;

        if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
            LOGERROR("EcsSerializer::Serialize: failed to write '", virtualPath, "'.");
    }

    void EcsSerializer::Deserialize(const ComponentSerializerRegistry& registry,
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
            return;
        }

        try
        {
            YAML::Node data = YAML::Load(*text);

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
            LOGERROR("Failed to parse scene: ", virtualPath, " with error: ", e.what());
            return;
        }
    }
}
