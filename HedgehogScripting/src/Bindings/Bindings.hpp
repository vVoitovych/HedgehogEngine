#pragma once

#include "HedgehogScripting/api/Sol.hpp"

#include <cstdint>
#include <vector>

namespace HedgehogEngine
{
    class EngineContext;
    struct FixedStepClock;
}

// The engine types and functions the ScriptSystem gives its scripts, as globals.
namespace HedgehogScripting::Bindings
{
    struct ScriptEntity;

    // Vector3 (HM::Vector3) and Quat (HM::Quaternion) usertypes.
    void RegisterMath(sol::state& lua);

    // Log.info / Log.warn / Log.error, and print routed to Log.info, all through Logger with the
    // calling script's file and line as prefix.
    void RegisterLog(sol::state& lua);

    // Entity (ScriptEntity) and Transform (ScriptComponentRef<TransformComponent>) usertypes over
    // context's ECS; Transform writes publish TransformChangedEvent on its EventBus. Entity also
    // gets getX/hasX/addX for the Light, Camera and Mesh components. context must outlive lua.
    void RegisterEntity(sol::state& lua, HedgehogEngine::EngineContext& context);

    // Light, Camera and Mesh (ScriptComponentRef<T>) usertypes and the LightType,
    // CameraProjectionType and CameraTargetMode enum tables. Writes go straight to the ECS;
    // castShadows goes through LightSystem::SetShadowCasting and mesh.path through
    // MeshSystem::LoadMesh, as in the inspector. context must outlive lua.
    void RegisterComponents(sol::state& lua, HedgehogEngine::EngineContext& context);

    // The Scene table (find, findAll, spawn, destroy) and entity:destroy(), over context's
    // SceneManager. Destroying only queues the entity in pendingDestroys; FlushDestroys deletes
    // them. Call after RegisterEntity. context and pendingDestroys must outlive lua.
    void RegisterScene(sol::state& lua, HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys);

    // Deletes the queued entities still alive through SceneManager::DeleteGameObject, which moves
    // their children up to the grandparent, and empties the queue. A scripted one gets OnDestroy
    // through the ScriptComponent removal callback.
    void FlushDestroys(HedgehogEngine::EngineContext& context, std::vector<ScriptEntity>& pendingDestroys);

    // The Time table: deltaTime (the hook's dt: OnUpdate's scaled dt, or the fixed dt inside
    // OnFixedUpdate), fixedDeltaTime, time (the clock's scaled simulated time), frame (OnUpdate
    // calls since Play) and timeScale, the only writable field, clamped to [0, 100] and stored in
    // the clock. Every read sees the current values. All three must outlive lua.
    void RegisterTime(sol::state& lua, HedgehogEngine::FixedStepClock& clock, const float& deltaTime,
                      const uint64_t& frame);
}
