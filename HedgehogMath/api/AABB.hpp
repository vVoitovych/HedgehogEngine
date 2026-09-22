#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

#include <array>

namespace HM
{
    class Matrix4x4;

    class AABB
    {
    public:
        HEDGEHOG_MATH_API AABB();
        HEDGEHOG_MATH_API AABB(const Vector3& min, const Vector3& max);

        HEDGEHOG_MATH_API Vector3 GetCenter() const;
        HEDGEHOG_MATH_API Vector3 GetHalfExtents() const;
        HEDGEHOG_MATH_API Vector3 GetMin() const;
        HEDGEHOG_MATH_API Vector3 GetMax() const;

        HEDGEHOG_MATH_API std::array<Vector3, 8> GetCorners() const;

        HEDGEHOG_MATH_API void ExpandToInclude(const Vector3& point);

        // Transforms all 8 corners by matrix and returns their axis-aligned bounds —
        // not tight for a rotated box, but that's the standard world-AABB tradeoff.
        HEDGEHOG_MATH_API AABB Transform(const Matrix4x4& matrix) const;

    private:
        Vector3 m_Min;
        Vector3 m_Max;
    };

}
