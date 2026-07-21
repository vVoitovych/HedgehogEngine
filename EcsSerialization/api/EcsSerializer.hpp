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

        // In-memory variants (no filesystem). Used for the editor's Play-mode snapshot/restore so a
        // play session never touches disk or mutates the saved scene.
        ECS_SERIALIZATION_API static std::string SerializeToString(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            const std::string& sceneName);

        ECS_SERIALIZATION_API static bool DeserializeFromString(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            std::string& outSceneName,
            const std::string& text);
    };
}
