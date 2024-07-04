#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace Context
{
	struct Light
	{
		alignas(16) glm::vec3 mPosition;
		alignas(16) glm::vec3 mDirection;
		alignas(16) glm::vec3 mColor;
		alignas(16) glm::vec4 mData;		// type, intencity, radius, coneAngle
	};
}


	