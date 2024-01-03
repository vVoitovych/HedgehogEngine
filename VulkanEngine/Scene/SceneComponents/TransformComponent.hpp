#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
	class TransformComponent
	{
	public:
		glm::vec3 mPososition = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 mRotation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 mScale = glm::vec3(0.0f, 0.0f, 0.0f);

		glm::mat4 mObjMatrix;

	};
}

