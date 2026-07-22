#include "api/AABB.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HM
{
    AABB::AABB()
        : m_Min(0.0f, 0.0f, 0.0f)
        , m_Max(0.0f, 0.0f, 0.0f)
    {
    }

    AABB::AABB(const Vector3& min, const Vector3& max)
        : m_Min(min)
        , m_Max(max)
    {
    }

    Vector3 AABB::GetCenter() const
    {
        return (m_Min + m_Max) * 0.5f;
    }

    Vector3 AABB::GetHalfExtents() const
    {
        return (m_Max - m_Min) * 0.5f;
    }

    Vector3 AABB::GetMin() const
    {
        return m_Min;
    }

    Vector3 AABB::GetMax() const
    {
        return m_Max;
    }

    std::array<Vector3, 8> AABB::GetCorners() const
    {
        return {
            Vector3(m_Min.x(), m_Min.y(), m_Min.z()),
            Vector3(m_Max.x(), m_Min.y(), m_Min.z()),
            Vector3(m_Min.x(), m_Max.y(), m_Min.z()),
            Vector3(m_Max.x(), m_Max.y(), m_Min.z()),
            Vector3(m_Min.x(), m_Min.y(), m_Max.z()),
            Vector3(m_Max.x(), m_Min.y(), m_Max.z()),
            Vector3(m_Min.x(), m_Max.y(), m_Max.z()),
            Vector3(m_Max.x(), m_Max.y(), m_Max.z())
        };
    }

    void AABB::ExpandToInclude(const Vector3& point)
    {
        m_Min = Min(m_Min, point);
        m_Max = Max(m_Max, point);
    }

    bool AABB::IntersectRay(const Vector3& origin, const Vector3& dir, float& tNear) const
    {
        float tmin = -std::numeric_limits<float>::infinity();
        float tmax =  std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; ++axis)
        {
            const float o  = origin[axis];
            const float d  = dir[axis];
            const float mn = m_Min[axis];
            const float mx = m_Max[axis];

            if (std::abs(d) < 1e-8f)
            {
                // Ray parallel to this slab: miss if the origin is outside it.
                if (o < mn || o > mx)
                    return false;
            }
            else
            {
                const float inv = 1.0f / d;
                float t1 = (mn - o) * inv;
                float t2 = (mx - o) * inv;
                if (t1 > t2)
                    std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax)
                    return false;
            }
        }

        if (tmax < 0.0f) // box entirely behind the ray origin
            return false;

        tNear = tmin;
        return true;
    }

}

