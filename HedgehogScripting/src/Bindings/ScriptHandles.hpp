#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

// The handles scripts hold for engine objects. Pure data: an entity id and the generation it had
// when the handle was made. A handle never points into component storage, which moves on every
// swap-remove; it looks its component up again on every access.
namespace HedgehogScripting::Bindings
{
    // A script's reference to an entity. Once that entity is destroyed the handle is invalid for
    // good, even after the id is reused, so it never names a different entity.
    struct ScriptEntity
    {
        ECS::Entity Id         = ECS::INVALID_ENTITY;
        uint32_t    Generation = 0;
    };

    // One component of a handle's entity, such as entity.transform.
    template<typename T>
    struct ScriptComponentRef
    {
        ScriptEntity Entity;
    };

    [[nodiscard]] inline ScriptEntity MakeScriptEntity(const ECS::ECS& ecs, ECS::Entity entity)
    {
        return ScriptEntity{ entity, ecs.GetGeneration(entity) };
    }

    [[nodiscard]] inline bool IsValid(const ECS::ECS& ecs, const ScriptEntity& handle)
    {
        return handle.Id != ECS::INVALID_ENTITY && ecs.IsAlive(handle.Id) &&
               ecs.GetGeneration(handle.Id) == handle.Generation;
    }

    [[nodiscard]] inline std::string Describe(const ScriptEntity& handle)
    {
        return "Entity " + std::to_string(handle.Id) + " (generation " + std::to_string(handle.Generation) + ")";
    }

    // Throws std::runtime_error, which sol2 turns into a Lua error, when the handle is stale.
    inline void RequireValid(const ECS::ECS& ecs, const ScriptEntity& handle)
    {
        if (!IsValid(ecs, handle))
            throw std::runtime_error(Describe(handle) + " no longer exists");
    }

    // The component, looked up now. Throws, naming the entity (and the component, by its
    // reflected type name), when the handle is stale or the entity has no T.
    template<typename T>
    [[nodiscard]] T& Resolve(ECS::ECS& ecs, const ScriptComponentRef<T>& ref)
    {
        RequireValid(ecs, ref.Entity);
        if (!ecs.HasComponent<T>(ref.Entity.Id))
            throw std::runtime_error(Describe(ref.Entity) + " has no " + T::s_TypeName);
        return ecs.GetComponent<T>(ref.Entity.Id);
    }
}
