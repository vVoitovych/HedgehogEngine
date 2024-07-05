#pragma once

#include "HedgehogMath/Vector.hpp"
#include "HedgehogMath/Matrix.hpp"

namespace Scene
{
	class TransformComponent
	{
	public:
		HM::Vector3 mPososition = HM::Vector3(0.0f, 0.0f, 0.0f);
		HM::Vector3 mRotation = HM::Vector3(0.0f, 0.0f, 0.0f);
		HM::Vector3 mScale = HM::Vector3(1.0f, 1.0f, 1.0f);

		HM::Matrix4x4 mObjMatrix = HM::Matrix4x4();

	};
}

