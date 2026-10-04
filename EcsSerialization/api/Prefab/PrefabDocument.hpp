#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "yaml-cpp/yaml.h"

#include <optional>
#include <string>

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    // A .prefab file (epic HE-185): "Version: <PREFAB_FORMAT_VERSION>", then "Root", the subtree
    // document EcsSerializer::SerializeSubtree writes (its own Version, SourceIds and Subtree with
    // local ids, 0 the prefab's root), which EcsSerializer::InstantiateSubtree reads.
    inline constexpr int         PREFAB_FORMAT_VERSION = 1;
    inline constexpr const char* PREFAB_EXTENSION      = ".prefab";

    struct PrefabReadResult
    {
        std::optional<YAML::Node> Root;  // the subtree document, when it reads
        std::string               Error; // why it does not, when it does not
    };

    // The text of a prefab of subtreeRoot and its descendants.
    [[nodiscard]] ECS_SERIALIZATION_API std::string WritePrefab(const ComponentSerializerRegistry& registry,
                                                                const ECS::ECS&                    ecs,
                                                                ECS::Entity                        subtreeRoot);

    // Refuses malformed YAML, a Version that is not a positive integer or is newer than
    // PREFAB_FORMAT_VERSION, and a Root that is not a map holding a Subtree sequence.
    [[nodiscard]] ECS_SERIALIZATION_API PrefabReadResult ReadPrefab(const std::string& text);
}
