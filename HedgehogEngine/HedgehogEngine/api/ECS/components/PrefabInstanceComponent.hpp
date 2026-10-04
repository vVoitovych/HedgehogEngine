#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "ECS/api/Entity.hpp"

#include <cstdint>
#include <string>

namespace HedgehogEngine
{
    // Links an entity made by PrefabManager::Instantiate to the prefab node it came from: LocalId is
    // that node's local id (0 the prefab's root), InstanceRoot the instance's root entity, and only
    // the root carries the prefab's PrefabPath. Saved with the scene (whose entities are still
    // written in full).
HH_BEGIN_COMPONENT(PrefabInstanceComponent)
    HH_PROP_NAMED(std::string, PrefabPath,   "PrefabPath",   std::string{},       None)
    HH_PROP_NAMED(uint32_t,    LocalId,      "LocalId",      0u,                  None)
    HH_PROP_NAMED(ECS::Entity, InstanceRoot, "InstanceRoot", ECS::INVALID_ENTITY, EntityRef)
HH_END_COMPONENT(PrefabInstanceComponent)
}
