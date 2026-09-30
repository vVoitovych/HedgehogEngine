#pragma once

#include "HedgehogScripting/api/Sol.hpp"

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class EventBus;
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
    // ecs; Transform writes publish TransformChangedEvent on eventBus. Both must outlive lua.
    void RegisterEntity(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus);
}
