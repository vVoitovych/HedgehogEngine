#include "RenderSystem.hpp"

namespace Scene
{
	void RenderSystem::Update(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		auto& render = coordinator.GetComponent<RenderComponent>(entity);
		if (render.mMaterialIndex.has_value())
		{
			if (mMaterialPathes[render.mMaterialIndex.value()] != render.mMaterial)
			{
				UpdateMaterialPath(coordinator, entity);
			}
		}
		else
		{
			UpdateMaterialPath(coordinator, entity);
		}
	}

	void RenderSystem::UpdataSystem(ECS::Coordinator& coordinator)
	{
		for (auto entity : entities)
		{
			Update(coordinator, entity);
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

	RenderComponent& RenderSystem::GetRenderComponentByIndex(ECS::Coordinator& coordinator, size_t index) const
	{
		auto entity = entities[index];
		return coordinator.GetComponent<RenderComponent>(entity);
	}

	void RenderSystem::UpdateMaterialPath(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		auto& component = coordinator.GetComponent<RenderComponent>(entity);

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



