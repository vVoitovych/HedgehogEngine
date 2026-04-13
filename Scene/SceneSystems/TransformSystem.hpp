#pragma once

#include "ECS/System.hpp"
#include "ECS/ECS.hpp"

namespace Scene
{
    class TransformSystem : public ECS::System
    {
    public:
        void Update(ECS::ECS& ecs);
    };
}
