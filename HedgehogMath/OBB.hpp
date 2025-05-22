#pragma once
#include "Vector.hpp"

namespace HM
{
    class AABB;
    class Matrix4x4;

    class OBB
    {
    public:
        OBB();
        float ProjectedRadius(const Vector3& planeNormal) const;
        Vector3 GetCenter() const;

        static OBB FromAABB(const AABB& localAABB, const Matrix4x4& modelMatrix);

    private:
        Vector3 m_Center;
        Vector3 m_HalfExtents;
        Vector3 m_Axes[3];
    };

}