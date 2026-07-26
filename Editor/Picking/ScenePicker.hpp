#pragma once

#include "ECS/api/Entity.hpp"

#include <cstdint>
#include <optional>

namespace HedgehogEngine
{
    class Engine;
}

namespace Editor::Picking
{
    // Casts a ray from the editor camera through the given scene-view pixel and returns the nearest
    // mesh entity whose local AABB it hits (nullopt on a miss). Pixel coordinates are relative to the
    // scene-view image (origin top-left); viewWidth/Height are that image's size in pixels.
    std::optional<ECS::Entity> PickEntity(HedgehogEngine::Engine& context,
                                          float pixelX, float pixelY,
                                          float viewWidth, float viewHeight);
}
