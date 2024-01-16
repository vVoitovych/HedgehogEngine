#include "TransformSystem.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"

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

			glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), transform.mPososition);
			glm::mat4 rotationMatrix = 
				glm::rotate(glm::mat4(1.0f), glm::radians(transform.mRotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(transform.mRotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
				glm::rotate(glm::mat4(1.0f), glm::radians(transform.mRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.mScale);
			
			transform.mObjMatrix = translationMatrix * rotationMatrix * scaleMatrix;

		}
	}
}


