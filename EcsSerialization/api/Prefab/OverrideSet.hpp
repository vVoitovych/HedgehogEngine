#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <string>
#include <vector>

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    // One value a prefab instance holds differently from its prefab: a key of one component's
    // YAML map on the node with that local id. An empty Property is the whole component, which the
    // prefab's node does not have (a component added on the instance).
    struct PropertyOverride
    {
        uint32_t    LocalId = 0;
        std::string Component;
        std::string Property;
        YAML::Node  Value;
    };

    using OverrideSet = std::vector<PropertyOverride>;

    // The nodes of a subtree document by local id (an absent id left as an undefined node).
    [[nodiscard]] ECS_SERIALIZATION_API std::vector<YAML::Node> IndexSubtree(const YAML::Node& subtreeDocument);

    // Equal YAML, numbers compared as the floats components hold ("0.15" equals "0.150000006"), so
    // a hand-written prefab does not read as overridden everywhere.
    [[nodiscard]] ECS_SERIALIZATION_API bool ValuesEqual(const YAML::Node& a, const YAML::Node& b);

    // What the instance (entities by local id, INVALID_ENTITY for none) holds differently from the
    // prefab's subtree document, per (local id, component, property), in local id and handler
    // order. skipComponent (the prefab link) is never compared. A component the prefab's node has
    // but the instance does not is not an override (v1): one warning names it.
    [[nodiscard]] ECS_SERIALIZATION_API OverrideSet DiffInstance(const ComponentSerializerRegistry& registry,
                                                                const ECS::ECS&                    ecs,
                                                                const std::vector<ECS::Entity>&    entities,
                                                                const YAML::Node&                  subtreeDocument,
                                                                const std::string&                 skipComponent);

    // Applies overrides to a freshly instantiated instance. One whose node, component or property
    // no longer exists in the prefab is dropped with a warning naming sourceName.
    ECS_SERIALIZATION_API void ApplyOverrides(const ComponentSerializerRegistry& registry,
                                              ECS::ECS&                          ecs,
                                              const std::vector<ECS::Entity>&    entities,
                                              const OverrideSet&                 overrides,
                                              const std::string&                 sourceName);

    // As a scene writes them: a sequence of { LocalId, Component, Property (absent for a whole
    // component), Value }.
    ECS_SERIALIZATION_API void WriteOverrides(YAML::Emitter& out, const OverrideSet& overrides);
    // Entries that do not read are skipped with a warning naming sourceName.
    [[nodiscard]] ECS_SERIALIZATION_API OverrideSet ReadOverrides(const YAML::Node& node, const std::string& sourceName);
}
