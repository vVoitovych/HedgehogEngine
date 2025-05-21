#include "Matrix.hpp"
#include "AABB.hpp"

namespace HM
{
	HM::AABB::AABB()
		: m_Min(0.0f, 0.0f, 0.0f)
		, m_Max(0.0f, 0.0f, 0.0f)
	{
	}

	AABB::AABB(const Vector3& v)
		: m_Min(0.0f, 0.0f, 0.0f)
		, m_Max(v)
	{
	}

	AABB::AABB(const Vector3& a, const Vector3& b)
		: m_Min(a)
		, m_Max(b)
	{
	}

	void AABB::Extend(const Vector3& v)
	{
		m_Min = Min(m_Min, v);
		m_Max = Max(m_Max, v);
	}

	void AABB::Extend(const AABB& ab)
	{
		m_Min = Min(m_Min, ab.m_Min);
		m_Max = Max(m_Max, ab.m_Max);
	}

	bool AABB::IsEmpty() const
	{
		auto diff = m_Max - m_Min;
		return !(diff.x() > 0 && diff.y() > 0 && diff.z() > 0);
	}

	AABB AABB::Intersect(const AABB& a, const AABB& b)
	{
		return AABB(Max(a.m_Min, b.m_Min), Min(a.m_Max, b.m_Max));
	}

	bool AABB::Overlaps(const AABB& a, const AABB& b)
	{
		return !Intersect(a, b).IsEmpty();
	}

}


