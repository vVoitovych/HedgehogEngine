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
		bool m_Enable = true;
		LightType m_LightType = LightType::DirectionLight;
		HM::Vector3 m_Color = { 1.0f, 1.0f, 1.0f };
		float m_Intencity = 1.0f;
		float m_Radius = 1.0f;
		float m_ConeAngle = 1.0f;

		bool m_CastShadows = false;

		HM::Vector3 m_Position = HM::Vector3(0.0f, 0.0f, 0.0f);
		HM::Vector3 m_Direction = HM::Vector3(0.0f, 0.0f, 0.0f);
	};
}


