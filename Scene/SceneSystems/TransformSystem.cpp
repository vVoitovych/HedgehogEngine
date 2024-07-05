#include "TransformSystem.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace Scene
{
	void TransformSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& transform = coordinator.GetComponent<TransformComponent>(entity);

			glm::mat4 matrix = glm::mat4(1.0f);
			matrix = glm::translate(matrix, transform.mPososition);
			matrix = glm::rotate(matrix, glm::radians(transform.mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
			matrix = glm::rotate(matrix, glm::radians(transform.mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			matrix = glm::rotate(matrix, glm::radians(transform.mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			matrix = glm::scale(matrix, transform.mScale);
			
			transform.mObjMatrix = matrix;

		}
	}
}


