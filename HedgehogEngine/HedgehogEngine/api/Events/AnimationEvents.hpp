#pragma once

#include "ECS/api/Entity.hpp"

#include <string>

namespace HedgehogEngine
{
    /// Emitted by AnimationSystem when a non-looping clip reaches its end: once per time it is
    /// played through.
    struct AnimationFinishedEvent
    {
        ECS::Entity Entity = 0;
        std::string Clip;
    };
}
