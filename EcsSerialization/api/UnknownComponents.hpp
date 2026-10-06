#pragma once

#include "EcsSerializationApi.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace EcsSerialization
{
    // A component an entity node names but no registered handler reads (its plugin is not loaded):
    // its key and its value as emitted YAML text, so the data survives until a type reads it.
    struct UnknownComponent
    {
        std::string Key;
        std::string Yaml;
    };

    // The unknown components of one entity, in document order. When the ECS has this type
    // registered, reading a scene, snapshot, save or prefab keeps every unknown component here and
    // writing it puts them back after the known ones; without it they are dropped (with the same
    // warning). Entity ids inside the data are not remapped when a subtree is instantiated.
    struct UnknownComponentsComponent
    {
        std::vector<UnknownComponent> Entries;
    };

    // The key the store type is registered under; it is never written to a document.
    inline constexpr const char* UNKNOWN_COMPONENTS_KEY = "UnknownComponents";

    // Whether key is one an entity node uses for itself (Entity, Name, Parent, Children, and a prefab
    // instance's Prefab, Entities, Overrides and PrefabOrigin) rather than for a component.
    [[nodiscard]] ECS_SERIALIZATION_API bool IsReservedEntityKey(std::string_view key);
}
