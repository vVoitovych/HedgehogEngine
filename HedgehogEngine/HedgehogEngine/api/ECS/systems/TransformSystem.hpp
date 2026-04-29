#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

namespace Scene
{
    class TransformSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void Update(ECS::ECS& ecs);
    };
}
