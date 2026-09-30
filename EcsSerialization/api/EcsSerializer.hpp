#pragma once

#include "EcsSerializationApi.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    class EcsSerializer
    {
    public:
        // The scene under ecs.GetRoot() as a YAML document. The file version writes
        // exactly this text.
        [[nodiscard]] ECS_SERIALIZATION_API static std::string SerializeToString(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            const std::string& sceneName);

        // Recreates every entity in yamlText with its saved id and sets the root.
        // sourceName only labels log messages. Returns false on malformed YAML; the
        // ECS may then hold part of the scene.
        [[nodiscard]] ECS_SERIALIZATION_API static bool DeserializeFromString(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            std::string& outSceneName,
            const std::string& yamlText,
            const std::string& sourceName);

        ECS_SERIALIZATION_API static bool Serialize(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            const std::string& sceneName,
            const std::string& virtualPath,
            const FS::FileSystemManager& fileSystem);

        ECS_SERIALIZATION_API static bool Deserialize(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            std::string& outSceneName,
            const std::string& virtualPath,
            const FS::FileSystemManager& fileSystem);
    };
}
