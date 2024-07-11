#pragma once

#include "HedgehogMath/Vector.hpp"

namespace Scene
{
	enum class LightType
	{
		DirectionLight = 0, 
		PointLight = 1,
		SpotLight = 2
	};

	class LightComponent
	{
	public:
		bool mEnable = true;
		LightType mLightType = LightType::DirectionLight;
		HM::Vector3 mColor = { 1.0f, 1.0f, 1.0f };
		float mIntencity = 1.0f;
		float mRadius = 1.0f;
		float mConeAngle = 1.0f;

		HM::Vector3 mPosition = HM::Vector3(0.0f, 0.0f, 0.0f);
		HM::Vector3 mDirection = HM::Vector3(0.0f, 0.0f, 0.0f);
	};
}


