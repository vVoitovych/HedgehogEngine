#include "RenderSystem.hpp"

namespace Scene
{
	void RenderSystem::Update(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		auto& render = coordinator.GetComponent<RenderComponent>(entity);
		if (render.mMaterialIndex.has_value())
		{
			if (mMaterialInstancePathes[render.mMaterialIndex.value()] != render.mMaterialInstance)
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
		return mMaterialInstancePathes.size();
	}

	const std::vector<std::string>& RenderSystem::GetMaterials() const
	{
		return mMaterialInstancePathes;
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

		if (!component.mMaterialInstance.empty())
		{
			auto it = std::find(mMaterialInstancePathes.begin(), mMaterialInstancePathes.end(), component.mMaterialInstance);
			if (it != mMaterialInstancePathes.end())
			{
				component.mMaterialIndex = it - mMaterialInstancePathes.begin();
			}
			else
			{
				component.mMaterialIndex = mMaterialInstancePathes.size();
				mMaterialInstancePathes.push_back(component.mMaterialInstance);
			}

		}
	}

}



