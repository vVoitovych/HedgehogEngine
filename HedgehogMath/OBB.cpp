#include "OBB.hpp"
#include "AABB.hpp"
#include "Matrix.hpp"

#include <algorithm>

namespace HM
{
	OBB::OBB()
		: m_Center(0.0f, 0.0f, 0.0f)
		, m_HalfExtents(0.0f, 0.0f, 0.0f)
	{
		m_Axes[0] = Vector3(0.0f, 0.0f, 0.0f);
		m_Axes[1] = Vector3(0.0f, 0.0f, 0.0f);
		m_Axes[2] = Vector3(0.0f, 0.0f, 0.0f);
	}

	float OBB::ProjectedRadius(const Vector3& planeNormal) const
	{
		return
			m_HalfExtents.x() * std::abs(Dot(planeNormal, m_Axes[0])) +
			m_HalfExtents.y() * std::abs(Dot(planeNormal, m_Axes[1])) +
			m_HalfExtents.z() * std::abs(Dot(planeNormal, m_Axes[2]));
	}

	Vector3 OBB::GetCenter() const
	{
		return m_Center;
	}


	OBB OBB::FromAABB(const AABB& localAABB, const Matrix4x4& modelMatrix)
	{
		OBB obb;
		obb.m_Center = Vector3(Vector4(localAABB.GetCenter(), 1.0f) * modelMatrix);
		obb.m_HalfExtents = localAABB.GetHalfExtents();

		obb.m_Axes[0] = Vector3(modelMatrix[0]).Normalize();
		obb.m_Axes[1] = Vector3(modelMatrix[1]).Normalize();
		obb.m_Axes[2] = Vector3(modelMatrix[2]).Normalize();

		return obb;
	}

}


