#pragma once

#include "CameraMath.hpp"
#include "RenderScene.hpp"

#include "HedgehogMath/api/AABB.hpp"

#include <cstdint>
#include <optional>

namespace HX
{
    // The distance along ray to where it enters box (0 when it starts inside), or nullopt when it
    // misses or the box is behind it. Ray.Direction must be unit length.
    [[nodiscard]] std::optional<float> IntersectRayAABB(const Ray& ray, const HM::AABB& box);

    // Editor picking: the instance whose world bounds the ray enters first, among those on a layer
    // in layerMask, as its SourceId (the ECS entity). Reads only the RenderScene: never the ECS, the
    // GPU or mesh data. nullopt when the ray hits nothing.
    [[nodiscard]] std::optional<uint64_t> PickInstance(const RenderScene& scene, const Ray& ray,
                                                       uint32_t layerMask = ~EDITOR_LAYER_MASK);
}
