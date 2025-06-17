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
			m_LightComponents[index++] = light;

		}
		return m_LightComponents;
	}

	void LightSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& light = coordinator.GetComponent<LightComponent>(entity);
			auto& transform = coordinator.GetComponent<TransformComponent>(entity);
			auto& transformMatrix = transform.mObjMatrix;
			light.m_Position = transform.mPososition;
			HM::Vector3 dir = { transformMatrix[0][0], transformMatrix[0][1], transformMatrix[0][2] };
			light.m_Direction = dir;
			if (light.m_CastShadows)
			{
				m_ShadowDirection = dir;
			}
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

	void LightSystem::SetShadowCasting(const ECS::Coordinator& coordinator, ECS::Entity inEntity, bool isCast)
	{
		m_ShadowDirection.reset();
		for (auto const& entity : entities)
		{
			auto& light = coordinator.GetComponent<LightComponent>(entity);
			light.m_CastShadows = false;
			if (entity == inEntity)
			{
				if (isCast)
				{
					light.m_CastShadows = isCast;
					m_ShadowDirection = light.m_Direction;
				}
			}

		}
	}

	const std::optional< HM::Vector3>& LightSystem::GetShadowDir() const
	{
		return m_ShadowDirection;
	}

}



