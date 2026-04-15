#pragma once

#include "ECS/api/System.hpp"
#include "ECS/api/ECS.hpp"

namespace Scene
{
    class TransformSystem : public ECS::System
    {
    public:
        void Update(ECS::ECS& ecs);
    };
}
