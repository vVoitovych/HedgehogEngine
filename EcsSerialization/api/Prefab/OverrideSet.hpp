#pragma once

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"
#include "EcsSerialization/api/EcsSerializationApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <string>
#include <vector>

namespace EcsSerialization
{
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

    class IPrefabProvider;

    // The entities of the instance rooted at root by local id (INVALID_ENTITY for a gap): root and
    // every descendant linked to root, reached through such entities. With added, also the entities
    // added under the instance (a child of one of its entities that is not one itself), not their
    // descendants.
    ECS_SERIALIZATION_API void CollectInstance(const IPrefabProvider& prefabs, const ECS::ECS& ecs, ECS::Entity root,
                                               std::vector<ECS::Entity>& members, std::vector<ECS::Entity>* added = nullptr);

    // The nodes of a subtree document by local id (an absent id left as an undefined node).
    [[nodiscard]] ECS_SERIALIZATION_API std::vector<YAML::Node> IndexSubtree(const YAML::Node& subtreeDocument);

    // Equal YAML, numbers compared as the floats components hold ("0.15" equals "0.150000006"), so
    // a hand-written prefab does not read as overridden everywhere.
    [[nodiscard]] ECS_SERIALIZATION_API bool ValuesEqual(const YAML::Node& a, const YAML::Node& b);

    // What the instance (entities by local id, INVALID_ENTITY for none) holds differently from the
    // prefab's subtree document, per (local id, component, property), in local id and handler
    // order. skipComponent (the prefab link) is never compared. Entity ids are compared through each
    // handler's RemapYaml (MakePrefabToInstanceRemap), so a reference to one of the instance's own
    // entities, or a cleared one to an entity outside the prefab, is not an override. A component
    // the prefab's node has but the instance does not is not an override (v1): one warning names it.
    [[nodiscard]] ECS_SERIALIZATION_API OverrideSet DiffInstance(const ComponentSerializerRegistry& registry,
                                                                const ECS::ECS&                    ecs,
                                                                const std::vector<ECS::Entity>&    entities,
                                                                const YAML::Node&                  subtreeDocument,
                                                                const std::string&                 skipComponent);

    // Maps a prefab document's entity ids (its SourceIds) to the instance's (entities by local id);
    // any other id becomes INVALID_ENTITY, as instantiating leaves it.
    [[nodiscard]] ECS_SERIALIZATION_API EntityRemap MakePrefabToInstanceRemap(const YAML::Node&               subtreeDocument,
                                                                             const std::vector<ECS::Entity>& entities);
    // The other way, for values applied to the prefab: an instance's entity becomes its source id,
    // any other id INVALID_ENTITY.
    [[nodiscard]] ECS_SERIALIZATION_API EntityRemap MakeInstanceToPrefabRemap(const YAML::Node&               subtreeDocument,
                                                                             const std::vector<ECS::Entity>& entities);

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
