#pragma once

#include "ECS/System.h"
#include "ECS/Coordinator.h"

#include "Scene/SceneComponents/RenderComponent.hpp"

#include <unordered_map>

namespace Scene
{
	enum class MaterialType
	{
		Opaque,
		CutOff,
		Transparent
	};

	struct Material
	{
		MaterialType type;
		std::string baseColor;
		float transparency;

	};

	class RenderSystem : public ECS::System
	{
	public:
		void Update(ECS::Coordinator& coordinator, ECS::Entity entity);

		void CreateMaterial();

		void LoadMaterial(ECS::Coordinator& coordinator, ECS::Entity entity);

		const std::unordered_map<std::string, Material>& GetMaterials() const;

	private:
		Material CreateDefaultMaterial();

		void UpdateMaterialPath(ECS::Coordinator& coordinator, ECS::Entity entity);

	private:
		std::unordered_map<std::string, Material> mMaterials;

	};


}


