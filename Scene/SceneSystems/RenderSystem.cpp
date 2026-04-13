#include "RenderSystem.hpp"

namespace Scene
{
	void RenderSystem::Update(ECS::ECS& ecs, ECS::Entity entity)
	{
		auto& render = ecs.GetComponent<RenderComponent>(entity);
		if (render.mMaterialIndex.has_value())
		{
			if (mMaterialPathes[render.mMaterialIndex.value()] != render.mMaterial)
			{
				UpdateMaterialPath(ecs, entity);
			}
		}
		else
		{
			UpdateMaterialPath(ecs, entity);
		}
	}

	void RenderSystem::UpdataSystem(ECS::ECS& ecs)
	{
		for (auto entity : entities)
		{
			Update(ecs, entity);
		}
	}

	size_t RenderSystem::GetMaterialsCount() const
	{
		return mMaterialPathes.size();
	}

	const std::vector<std::string>& RenderSystem::GetMaterials() const
	{
		return mMaterialPathes;
	}

	const std::vector<ECS::Entity>& RenderSystem::GetEntities() const
	{
		return entities;
	}

	RenderComponent& RenderSystem::GetRenderComponentByIndex(ECS::ECS& ecs, size_t index) const
	{
		auto entity = entities[index];
		return ecs.GetComponent<RenderComponent>(entity);
	}

	void RenderSystem::UpdateMaterialPath(ECS::ECS& ecs, ECS::Entity entity)
	{
		auto& component = ecs.GetComponent<RenderComponent>(entity);

		if (!component.mMaterial.empty())
		{
			auto it = std::find(mMaterialPathes.begin(), mMaterialPathes.end(), component.mMaterial);
			if (it != mMaterialPathes.end())
			{
				component.mMaterialIndex = it - mMaterialPathes.begin();
			}
			else
			{
				component.mMaterialIndex = mMaterialPathes.size();
				mMaterialPathes.push_back(component.mMaterial);
			}

		}
	}

}



