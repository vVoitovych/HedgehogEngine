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


		void CreateMaterial();

		void LoadMaterial(ECS::Coordinator& coordinator, ECS::Entity entity);

		const std::vector<std::string>& GetMaterials() const;

	private:
		void UpdateMaterialPath(ECS::Coordinator& coordinator, ECS::Entity entity);

	private:
		std::vector<std::string> mMaterialPathes;

	};


}


