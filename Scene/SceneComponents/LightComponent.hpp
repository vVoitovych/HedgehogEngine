#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL

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
		bool mEnable = true;
		LightType mLightType = LightType::DirectionLight;
		glm::vec3 mColor = { 1.0f, 1.0f, 1.0f };
		float mIntencity = 1.0f;
		float mRadius = 1.0f;
		float mConeAngle = 1.0f;

		glm::vec3 mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 mDirection = glm::vec3(0.0f, 0.0f, 0.0f);
	};
}


