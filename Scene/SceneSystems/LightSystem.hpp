#pragma once

#include "ECS/Coordinator.h"
#include "ECS/Entity.h"
#include "Scene/SceneComponents/LightComponent.hpp"

#include <vector>
#include <optional>

namespace Scene
{
	class LightSystem : public ECS::System
	{
	public:
		const std::vector<LightComponent>& GetLightComponents(ECS::Coordinator& coordinator);
		void Update(ECS::Coordinator& coordinator);
		size_t GetLightComponentsCount() const;
		const LightComponent& GetLightComponentByIndex(const ECS::Coordinator& coordinator, size_t index) const;

		void SetShadowCasting(const ECS::Coordinator& coordinator, ECS::Entity inEntity, bool isCast);
		const std::optional< HM::Vector3>& GetShadowDir() const;

	private:
		std::vector<LightComponent> m_LightComponents;
		std::optional<HM::Vector3> m_ShadowDirection;

	};

}