#pragma once
#include "Vector.hpp"
#include "HedgehogMathAPI.hpp"

namespace HM
{
    class AABB;
    class Matrix4x4;

    class OBB
    {
    public:
        HEDGEHOG_MATH_API OBB();
        HEDGEHOG_MATH_API float ProjectedRadius(const Vector3& planeNormal) const;
        HEDGEHOG_MATH_API Vector3 GetCenter() const;

        HEDGEHOG_MATH_API static OBB FromAABB(const AABB& localAABB, const Matrix4x4& modelMatrix);

    private:
        Vector3 m_Center;
        Vector3 m_HalfExtents;
        Vector3 m_Axes[3];
    };

}
