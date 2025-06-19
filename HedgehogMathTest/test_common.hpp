#pragma once

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"

namespace HedgehogTest
{
	bool NearlyEqual(float a, float b, float epsilon = 1e-5f);

	bool NearlyEqual(const HM::Vector3& a, const HM::Vector3& b, float epsilon = 1e-5f);

	bool NearlyEqual(const HM::Vector4& a, const HM::Vector4& b, float epsilon = 1e-5f);

	bool NearlyEqual(const HM::Matrix4x4& a, const HM::Matrix4x4& b, float epsilon = 1e-5f);
}

