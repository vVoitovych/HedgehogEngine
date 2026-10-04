#pragma once

#include "EcsSerializationApi.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

// Callers of the node functions include yaml-cpp themselves.
namespace YAML
{
    class Node;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    // What InstantiateSubtree does with a reference to an entity outside the subtree.
    enum class ExternalReferences
    {
        Keep,  // kept while that entity is alive (a copy within one scene)
        Clear  // always INVALID_ENTITY (a prefab, which names nothing outside itself)
    };

    struct InstantiateOptions
    {
        ExternalReferences External = ExternalReferences::Keep;
        // When set, filled with the new entity of each local id (index = local id).
        std::vector<ECS::Entity>* LocalEntities = nullptr;
    };

    // Scenes, snapshots, saves and (through the subtree functions) prefabs share one path: reading parses text into a
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

        // One entity and its descendants as a document of the same format: "Version", then
        // "SourceIds" (local id i was SourceIds[i] in the ECS it came from), then "Subtree", the
        // per-entity YAML scenes use with dense local ids in depth-first order (0 is the subtree's
        // root, whose Parent is INVALID_ENTITY). Component data keeps the source ids it refers to.
        [[nodiscard]] ECS_SERIALIZATION_API static std::string SerializeSubtreeToString(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            ECS::Entity subtreeRoot);

        [[nodiscard]] ECS_SERIALIZATION_API static YAML::Node SerializeSubtree(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            ECS::Entity subtreeRoot);

        // Creates a fresh entity for each one in a subtree document, rebuilds the hierarchy
        // through them under parent (appended last to its children) and points every entity
        // reference (an EntityRef property, or what a handler's RemapEntities rewrites) at the
        // new copy when it names an entity of the subtree; one outside it is kept while that
        // entity is alive (unless options.External clears it) and becomes INVALID_ENTITY otherwise. Returns the new root, or
        // INVALID_ENTITY, logging why, for a parent without a hierarchy, a document of an unknown
        // version, a malformed one or one that does not fit in MAX_ENTITIES; nothing is left
        // created then. Whole scenes are unaffected.
        [[nodiscard]] ECS_SERIALIZATION_API static ECS::Entity InstantiateSubtree(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            const YAML::Node& document,
            ECS::Entity parent,
            const std::string& sourceName,
            const InstantiateOptions& options = {});
    };
}
