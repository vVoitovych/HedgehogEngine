#pragma once

#include "ECS/api/Entity.hpp"

namespace HedgehogEngine
{
    /// Published once when a NavAgentComponent reaches its destination.
    struct NavAgentArrivedEvent
    {
        ECS::Entity Entity = ECS::INVALID_ENTITY;
    };
}
