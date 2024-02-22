#pragma once

#define MAX_LIGHT_COUNT

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
		void UpdateLights(ECS::Coordinator& coordinator);
		size_t GetLightComponentsCount() const;

	private:
		std::vector<LightComponent> mLightComponents;
	};

}