#pragma once

#include "ECS/api/System.hpp"

namespace HedgehogEngine
{
    // Entity view over all entities carrying a CameraComponent. Signature-tracked by the
    // SystemManager; FrameDataBuilder walks GetEntities() to resolve the primary game camera.
    class CameraSystem : public ECS::System
    {
    };
}
