#pragma once

#include "ECS/System.h"
#include "ECS/ECS.h"

#include "Scene/SceneComponents/RenderComponent.hpp"

namespace Scene
{
	class RenderSystem : public ECS::System
	{
	public:
		void Update(ECS::ECS& ecs, ECS::Entity entity);
		void UpdataSystem(ECS::ECS& ecs);
		size_t GetMaterialsCount() const;
		const std::vector<std::string>& GetMaterials() const;

		RenderComponent& GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const;

		const std::vector<ECS::Entity>& GetEntities() const;

	private:
		void UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity);

	private:
		std::vector<std::string> mMaterialPathes;

	};


}


