#pragma once

#include "Plane.hpp"
#include "HedgehogMathAPI.hpp"

namespace HM
{
    class Matrix4x4;
    class AABB;
    class OBB;

    class Frustum
    {
    public:
        enum Planes
        {
            Near = 0,
            Far,
            Left,
            Right,
            Top,
            Bottom,
            Count
        };

        HEDGEHOG_MATH_API void ExtractPlanes(const Matrix4x4& m, bool normalize = true);

        HEDGEHOG_MATH_API bool IsAABBVisible(const AABB& box) const;
        HEDGEHOG_MATH_API bool IsOBBVisible(const OBB& obb) const;

    private:
        Plane m_Planes[Count];

    };
}
