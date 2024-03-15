#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
	class TransformComponent
	{
	public:
		glm::vec3 mPososition = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 mRotation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 mScale = glm::vec3(1.0f, 1.0f, 1.0f);

		glm::mat4 mObjMatrix = glm::mat4(0.0f);

	};
}

