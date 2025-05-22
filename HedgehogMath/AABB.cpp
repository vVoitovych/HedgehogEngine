#include "AABB.hpp"

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

}

