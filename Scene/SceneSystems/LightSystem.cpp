#include "LightSystem.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

namespace Scene
{
	const std::vector<LightComponent>& LightSystem::GetLightComponents(ECS::Coordinator& coordinator)
	{
		size_t index = 0;
		for (auto const& entity : entities)
		{
			auto& light = coordinator.GetComponent<LightComponent>(entity);
			mLightComponents[index++] = light;

		}
		return mLightComponents;
	}

	void LightSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& light = coordinator.GetComponent<LightComponent>(entity);
			auto& transform = coordinator.GetComponent<TransformComponent>(entity);
			auto& transformMatrix = transform.mObjMatrix;
			light.mPosition = transform.mPososition;
			glm::vec3 dir = { transformMatrix[0][0], transformMatrix[0][1], transformMatrix[0][2] };
			light.mDirection = dir;
		}
	}

	size_t LightSystem::GetLightComponentsCount() const
	{
		return entities.size();
	}

	const LightComponent& LightSystem::GetLightComponentByIndex(const ECS::Coordinator& coordinator, size_t index) const
	{
		auto& entity = entities[index];
		return coordinator.GetComponent<LightComponent>(entity);
	}

}



