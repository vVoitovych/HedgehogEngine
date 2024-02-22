#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace Renderer
{
	struct Light
	{
		glm::vec4 mPosition;	// vec3 position, uint lightType
		glm::vec4 mDirection;	// vec3 direction, float radius
		glm::vec4 mColor;		// vec3 color, floar intencity
	};
}


	