#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

#include <array>

namespace HM
{
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

        // Slab ray/box intersection. dir need not be normalised. On a hit, tNear is the entry
        // parameter along dir (may be negative if origin is inside the box). Returns false when
        // the box is entirely behind the origin or the ray misses.
        HEDGEHOG_MATH_API bool IntersectRay(const Vector3& origin, const Vector3& dir, float& tNear) const;

    private:
        Vector3 m_Min;
        Vector3 m_Max;
    };

}
