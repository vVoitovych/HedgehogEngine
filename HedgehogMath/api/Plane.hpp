#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

namespace HM
{
    struct Plane
    {
        Vector3 normal;
        float distance;

        HEDGEHOG_MATH_API float GetSignedDistanceToPlane(const Vector3& point) const;
    };

}
