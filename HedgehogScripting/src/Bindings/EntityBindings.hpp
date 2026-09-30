#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/Entity.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

namespace ECS
{
    class ECS;
}

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class EventBus;
}

// Entities as scripts see them. Registered per ScriptSystem, since they reach into its ECS.
namespace HedgehogScripting::Bindings
{
    // A script's reference to an entity: its id and the generation the id had when the handle
    // was made. Once that entity is destroyed the handle is invalid for good, even after the id
    // is reused, so it never names a different entity.
    struct EntityHandle
    {
        ECS::Entity Id         = ECS::INVALID_ENTITY;
        uint32_t    Generation = 0;
    };

    // entity.transform: the TransformComponent of the handle's entity, looked up on every access.
    struct TransformHandle
    {
        EntityHandle Entity;
    };

    [[nodiscard]] EntityHandle MakeEntityHandle(const ECS::ECS& ecs, ECS::Entity entity);
    [[nodiscard]] bool         IsValid(const ECS::ECS& ecs, const EntityHandle& handle);
    // "Entity 5 (generation 2)".
    [[nodiscard]] std::string  DescribeEntity(const EntityHandle& handle);
    // Throws std::runtime_error naming the entity once it no longer exists.
    void                       RequireValid(const ECS::ECS& ecs, const EntityHandle& handle);

    // The handle's transform. Throws std::runtime_error, which sol2 turns into a Lua error,
    // naming the id when the entity no longer exists or has no transform.
    [[nodiscard]] HedgehogEngine::TransformComponent& GetTransform(ECS::ECS& ecs, const EntityHandle& handle);

    // The Entity and Transform usertypes, and the components below. Transform setters publish
    // TransformChangedEvent.
    void RegisterEntity(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                        const FS::FileSystemManager& fileSystem);

    // Light, Camera and Mesh usertypes, the LightType and CameraProjectionType enums, and
    // entity:get/has/add Light, Camera and Mesh (ComponentBindings.cpp).
    void RegisterComponents(sol::state& lua, sol::usertype<EntityHandle>& entityType, ECS::ECS& ecs,
                            const FS::FileSystemManager& fileSystem);

    // Which "<script>|<function>" deprecation warnings were already logged.
    using WarnedSet = std::shared_ptr<std::unordered_set<std::string>>;

    // GetPosition/SetPosition/GetRotation/SetRotation for one instance's own entity, as {x, y, z}
    // tables, kept for scripts written before the Entity API. Each logs a deprecation warning the
    // first time a script file uses it.
    void AddLegacyFunctions(sol::table& environment, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                            EntityHandle entity, const std::string& scriptPath, WarnedSet warned);
}
