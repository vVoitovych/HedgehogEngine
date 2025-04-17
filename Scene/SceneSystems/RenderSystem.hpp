#pragma once

#include "ECS/System.h"
#include "ECS/Coordinator.h"

#include "Scene/SceneComponents/RenderComponent.hpp"

namespace Scene
{
	class RenderSystem : public ECS::System
	{
	public:
		void Update(ECS::Coordinator& coordinator, ECS::Entity entity);
		void UpdataSystem(ECS::Coordinator& coordinator);
		size_t GetMaterialsCount() const;
		const std::vector<std::string>& GetMaterials() const;

		RenderComponent& GetRenderComponentByIndex(ECS::Coordinator& coordinator, size_t index) const;

		const std::vector<ECS::Entity>& GetEntities() const;

	private:
		void UpdateMaterialPath(ECS::Coordinator& coordinator, ECS::Entity entity);

	private:
		std::vector<std::string> mMaterialInstancePathes;

	};


}


