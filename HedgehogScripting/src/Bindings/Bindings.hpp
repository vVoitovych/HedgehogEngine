#pragma once

#include "HedgehogScripting/api/Sol.hpp"

namespace HedgehogEngine
{
    class EngineContext;
}

// The engine types and functions the ScriptSystem gives its scripts, as globals.
namespace HedgehogScripting::Bindings
{
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
}
