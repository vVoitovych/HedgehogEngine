#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    // A pure entity view over every UiCanvasComponent: the roots extraction walks to build the
    // frame's UI draw list. GetEntities() (inherited from ECS::System) is the whole interface for now.
    class UiSystem : public ECS::System
    {
    };
}
