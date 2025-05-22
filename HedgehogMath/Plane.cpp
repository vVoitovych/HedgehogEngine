#include "Plane.hpp"

namespace HM
{
	float Plane::GetSignedDistanceToPlane(const Vector3& point) const
	{
		return Dot(normal, point) + distance;
	}

}




