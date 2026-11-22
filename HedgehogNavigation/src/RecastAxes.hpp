#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <cstdint>

// The one place the engine's axes meet Recast's: the engine is Z-up, Recast and Detour are Y-up. A
// point swaps y and z both ways; the swap is a mirror, so a triangle counterclockwise from above in
// the engine is clockwise from above in Recast and has its winding reversed on the way in.
namespace HN
{
    inline std::array<float, 3> ToRecast(const HM::Vector3& point)
    {
        return { point.x(), point.z(), point.y() };
    }

    inline HM::Vector3 FromRecast(const float* point)
    {
        return HM::Vector3(point[0], point[2], point[1]);
    }

    inline std::array<int, 3> ToRecastTriangle(uint32_t a, uint32_t b, uint32_t c)
    {
        return { static_cast<int>(a), static_cast<int>(c), static_cast<int>(b) };
    }
}
