#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"

#include "yaml-cpp/yaml.h"

#include <memory>
#include <string>
#include <vector>

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
    class IPrefabProvider;

    // A prefab ready to instantiate, or why it is not.
    struct ExpandedPrefab
    {
        std::shared_ptr<const YAML::Node> Document;
        std::string                       Error;
    };

    // The prefab at path as one subtree document of full entities: every prefab instance written
    // inside it (a node with "Prefab", as a scene writes one) is replaced, recursively, by that
    // prefab's own expansion. The nested entities take the ids the outer prefab gave them in the
    // instance's "Entities" (a node the nested prefab gained gets a new id past the outer ones, so
    // local ids may leave gaps), their entity references are rewritten into the outer prefab's
    // ids, the instance's "Overrides" are applied, its name kept and its added entities attached
    // under their parents. Each nested entity carries "PrefabOrigin: [<instance node>, <its id in
    // the nested prefab>]", which applying a value to the outer prefab follows. A prefab holding
    // no instance is returned as read. Error names a prefab that cannot be read and a cycle with
    // its chain ("prefab cycle: a.prefab -> b.prefab -> a.prefab").
    [[nodiscard]] ECS_SERIALIZATION_API ExpandedPrefab ExpandPrefab(IPrefabProvider&                   prefabs,
                                                                    const ComponentSerializerRegistry& registry,
                                                                    const std::string&                 path);

    // The prefabs the document's instances name, directly (not through them).
    [[nodiscard]] ECS_SERIALIZATION_API std::vector<std::string> FindNestedPrefabs(const YAML::Node& subtreeDocument);

    // The chain from "from" through the prefabs it contains to "target" ("from", ..., "target"),
    // or empty when "from" does not contain "target" at any depth (unreadable prefabs are skipped).
    [[nodiscard]] ECS_SERIALIZATION_API std::vector<std::string> FindPrefabChain(IPrefabProvider&   prefabs,
                                                                               const std::string& from,
                                                                               const std::string& target);

    // "a.prefab -> b.prefab -> a.prefab".
    [[nodiscard]] ECS_SERIALIZATION_API std::string DescribePrefabChain(const std::vector<std::string>& chain);
}
