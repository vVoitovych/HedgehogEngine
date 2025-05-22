#include "Frustum.hpp"

#include "Matrix.hpp"
#include "AABB.hpp"
#include "OBB.hpp"

namespace HM
{
	void Frustum::ExtractPlanes(const Matrix4x4& m, bool normalize)
	{
        // Left
        m_Planes[Left].normal.x() = m[0][3] + m[0][0];
        m_Planes[Left].normal.y() = m[1][3] + m[1][0];
        m_Planes[Left].normal.z() = m[2][3] + m[2][0];
        m_Planes[Left].distance = m[3][3] + m[3][0];

        // Right
        m_Planes[Right].normal.x() = m[0][3] - m[0][0];
        m_Planes[Right].normal.y() = m[1][3] - m[1][0];
        m_Planes[Right].normal.z() = m[2][3] - m[2][0];
        m_Planes[Right].distance = m[3][3] - m[3][0];

        // Bottom
        m_Planes[Bottom].normal.x() = m[0][3] + m[0][1];
        m_Planes[Bottom].normal.y() = m[1][3] + m[1][1];
        m_Planes[Bottom].normal.z() = m[2][3] + m[2][1];
        m_Planes[Bottom].distance = m[3][3] + m[3][1];

        // Top
        m_Planes[Top].normal.x() = m[0][3] - m[0][1];
        m_Planes[Top].normal.y() = m[1][3] - m[1][1];
        m_Planes[Top].normal.z() = m[2][3] - m[2][1];
        m_Planes[Top].distance = m[3][3] - m[3][1];

        // Near
        m_Planes[Near].normal.x() = m[0][3] + m[0][2];
        m_Planes[Near].normal.y() = m[1][3] + m[1][2];
        m_Planes[Near].normal.z() = m[2][3] + m[2][2];
        m_Planes[Near].distance = m[3][3] + m[3][2];

        // Far
        m_Planes[Far].normal.x() = m[0][3] - m[0][2];
        m_Planes[Far].normal.y() = m[1][3] - m[1][2];
        m_Planes[Far].normal.z() = m[2][3] - m[2][2];
        m_Planes[Far].distance = m[3][3] - m[3][2];

        if (normalize) 
        {
            for (int i = 0; i < Count; i++) 
            {
                float length = m_Planes[i].normal.LengthSlow();
                m_Planes[i].normal /= length;
                m_Planes[i].distance /= length;
            }
        }
	}

	bool Frustum::IsAABBVisible(const AABB& box) const
	{
        for (int i = 0; i < Planes::Count; ++i)
        {
            const auto& plane = m_Planes[i];
            auto positiveVertex = box.GetMin();
            auto boxMax = box.GetMax();

            if (plane.normal.x() >= 0) positiveVertex.x() = boxMax.x();
            if (plane.normal.y() >= 0) positiveVertex.y() = boxMax.y();
            if (plane.normal.z() >= 0) positiveVertex.z() = boxMax.z();

            if (plane.GetSignedDistanceToPlane(positiveVertex) < 0)
                return false;
        }
        return true;
	}

    bool Frustum::IsOBBVisible(const OBB& obb) const
    {
        for (int i = 0; i < Planes::Count; ++i)
        {
            const auto& plane = m_Planes[i];
            float r = obb.ProjectedRadius(plane.normal);
            float d = Dot(plane.normal, obb.GetCenter()) + plane.distance;

            if (d + r < 0.0f)
                return false;
        }
        return true;
    }


}

