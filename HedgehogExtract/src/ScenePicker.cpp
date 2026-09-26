#include "api/ScenePicker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HX
{
    std::optional<float> IntersectRayAABB(const Ray& ray, const HM::AABB& box)
    {
        const HM::Vector3 min = box.GetMin();
        const HM::Vector3 max = box.GetMax();

        // The slab test: the ray is inside the box where it is between all three pairs of planes.
        float entry = 0.0f;
        float exit  = std::numeric_limits<float>::max();
        for (size_t axis = 0; axis < 3; ++axis)
        {
            const float origin    = ray.Origin[axis];
            const float direction = ray.Direction[axis];
            if (std::abs(direction) < 1e-8f)
            {
                // Parallel to this pair of planes: it misses unless it runs between them.
                if (origin < min[axis] || origin > max[axis])
                    return std::nullopt;
                continue;
            }
            float nearT = (min[axis] - origin) / direction;
            float farT  = (max[axis] - origin) / direction;
            if (nearT > farT)
                std::swap(nearT, farT);
            entry = std::max(entry, nearT);
            exit  = std::min(exit, farT);
            if (entry > exit)
                return std::nullopt;
        }
        return entry;
    }

    std::optional<uint64_t> PickInstance(const RenderScene& scene, const Ray& ray, uint32_t layerMask)
    {
        std::optional<uint64_t> picked;
        float                   nearest = std::numeric_limits<float>::max();
        for (const RenderInstance& instance : scene.Instances)
        {
            if (instance.Layer >= 32 || (layerMask & (1u << instance.Layer)) == 0)
                continue;
            const std::optional<float> distance = IntersectRayAABB(ray, instance.WorldBounds);
            if (distance && *distance < nearest)
            {
                nearest = *distance;
                picked  = instance.SourceId;
            }
        }
        return picked;
    }
}
