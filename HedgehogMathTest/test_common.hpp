#pragma once

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"

namespace HedgehogTest
{
	bool NearlyEqual(float a, float b);

	bool NearlyEqual(const HM::Vector3& a, const HM::Vector3& b);

	bool NearlyEqual(const HM::Vector4& a, const HM::Vector4& b);

	bool NearlyEqual(const HM::Matrix4x4& a, const HM::Matrix4x4& b);
}

