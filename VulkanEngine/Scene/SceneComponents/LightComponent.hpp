#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
		LightType mLightType;
		glm::vec3 mColor;
		float mIntencity;
		float mRadius;
		float mConeAngle;

		glm::vec3 mPosition;
		glm::vec3 mDirection;
	};
}


