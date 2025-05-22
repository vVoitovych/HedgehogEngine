#pragma once

#include "Plane.hpp"

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

        void ExtractPlanes(const Matrix4x4& m, bool normalize = true);

        bool IsAABBVisible(const AABB& box) const;
        bool IsOBBVisible(const OBB& obb) const;

    private:
        Plane m_Planes[Count];

    };
}

