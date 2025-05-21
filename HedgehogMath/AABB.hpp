#pragma once
#include "Vector.hpp"

namespace HM
{
	class AABB
	{
	public:
		AABB();
		AABB(const Vector3& v);
		AABB(const Vector3& a, const Vector3& b);

		void Extend(const Vector3& v);
		void Extend(const AABB& ab);

		bool IsEmpty() const;

		static AABB Intersect(const AABB& a, const AABB& b);
		static bool Overlaps(const AABB& a, const AABB& b);

	private:
		Vector3 m_Min;
		Vector3 m_Max;
	};

}

