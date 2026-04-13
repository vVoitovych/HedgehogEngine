#pragma once

#include "ECS/ECS.h"
#include "ECS/Entity.h"
#include "Scene/SceneComponents/LightComponent.hpp"

#include <vector>
#include <optional>

namespace Scene
{
	class LightSystem : public ECS::System
	{
	public:
		const std::vector<LightComponent>& GetLightComponents(ECS::ECS& ecs);
		void Update(ECS::ECS& ecs);
		size_t GetLightComponentsCount() const;
		const LightComponent& GetLightComponentByIndex(const ECS::ECS& ecs, size_t index) const;

		void SetShadowCasting(const ECS::ECS& ecs, ECS::Entity inEntity, bool isCast);
		const std::optional< HM::Vector3>& GetShadowDir() const;

	private:
		std::vector<LightComponent> m_LightComponents;
		std::optional<HM::Vector3> m_ShadowDirection;

	};

}