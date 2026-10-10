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

    /// Emitted by AnimationSystem when an animator's controller enters another state, by a
    /// transition or by AnimationSystem::Play: once per change, From empty on the first state.
    struct AnimatorStateChangedEvent
    {
        ECS::Entity Entity = 0;
        std::string From;
        std::string To;
    };
}
