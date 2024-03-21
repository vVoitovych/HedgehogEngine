#pragma once

#include "ECS/Coordinator.h"
#include "ECS/Entity.h"
#include "Scene/SceneComponents/LightComponent.hpp"

#include <vector>

namespace Scene
{
	class LightSystem : public ECS::System
	{
	public:
		const std::vector<LightComponent>& GetLightComponents(ECS::Coordinator& coordinator);
		void Update(ECS::Coordinator& coordinator);
		size_t GetLightComponentsCount() const;
		const LightComponent& GetLightComponentByIndex(const ECS::Coordinator& coordinator, size_t index) const;
	private:
		std::vector<LightComponent> mLightComponents;
	};

}