#pragma once

#include "HedgehogMath/Vector.hpp"

namespace Context
{
	struct Light
	{
		alignas(16) HM::Vector3 mPosition;
		alignas(16) HM::Vector3 mDirection;
		alignas(16) HM::Vector3 mColor;
		alignas(16) HM::Vector4 mData;		// type, intencity, radius, coneAngle
	};
}


	