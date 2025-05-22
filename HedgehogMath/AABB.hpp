#pragma once
#include "Vector.hpp"

#include <array>

namespace HM
{
	class AABB
	{
	public:
        AABB();
        AABB(const Vector3& min, const Vector3& max);

        Vector3 GetCenter() const;
        Vector3 GetHalfExtents() const;
        Vector3 GetMin() const;
        Vector3 GetMax() const;

        std::array<Vector3, 8> GetCorners() const;

        void ExpandToInclude(const Vector3& point);

	private:
        Vector3 m_Min;
        Vector3 m_Max;
	};

}

