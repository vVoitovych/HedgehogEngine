#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

namespace Scene
{
    class ScriptComponent;

    class ScriptSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs, float dt);

        HEDGEHOG_ENGINE_API void ClearScriptComponent(ECS::Entity entity, ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API void ChangeScript(ECS::Entity entity, ECS::ECS& ecs);
        HEDGEHOG_ENGINE_API void InitScript(ECS::Entity entity, ECS::ECS& ecs);

    private:
        void CallOnEnable(ECS::ECS& ecs);
        void CallUpdate(ECS::ECS& ecs, float dt);
        void CallOnDisable(ECS::ECS& ecs);
    };
}
