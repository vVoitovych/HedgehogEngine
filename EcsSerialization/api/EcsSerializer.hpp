#pragma once

#include "EcsSerializationApi.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

// Callers of the node functions include yaml-cpp themselves.
namespace YAML
{
    class Node;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    // Scenes, snapshots and (later) saves and prefabs share one path: reading parses text into a
    // node and reads that (DeserializeFromNode); writing emits text (SerializeToString), which
    // SerializeToNode parses; the file functions are thin wrappers over the string ones.
    //
    // A document starts with "Version: <FORMAT_VERSION>". One without a Version is version 1, which
    // every scene saved before versioning is. A document newer than FORMAT_VERSION, or whose
    // Version is not a positive integer, is refused before the ECS is touched.
    class EcsSerializer
    {
    public:
        static constexpr int FORMAT_VERSION = 1;

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

        // The same document as a node, to embed in a larger one (a save file).
        [[nodiscard]] ECS_SERIALIZATION_API static YAML::Node SerializeToNode(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            const std::string& sceneName);

        // DeserializeFromString over a parsed document. Returns false, logging why, for a
        // document of an unknown version (leaving the ECS untouched) or a malformed one (the
        // ECS may then hold part of the scene).
        [[nodiscard]] ECS_SERIALIZATION_API static bool DeserializeFromNode(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            std::string& outSceneName,
            const YAML::Node& document,
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
