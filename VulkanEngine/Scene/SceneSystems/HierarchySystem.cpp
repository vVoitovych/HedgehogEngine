#include "HierarchySystem.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

namespace Scene
{
	void HierarchySystem::Update(ECS::Coordinator& coordinator)
	{
		UpdateChildrenMatricies(coordinator, mRoot);
	}

	void HierarchySystem::SetRoot(ECS::Entity entity)
	{
		mRoot = entity;
	}

	void HierarchySystem::UpdateChildrenMatricies(ECS::Coordinator& coordinator, ECS::Entity parent)
	{
		auto& transform = coordinator.GetComponent<TransformComponent>(parent);
		auto& hierarchy = coordinator.GetComponent<HierarchyComponent>(parent);
		for (auto const& entity : hierarchy.mChildren)
		{
			auto& childTransform = coordinator.GetComponent<TransformComponent>(entity);
			childTransform.mObjMatrix = transform.mObjMatrix * childTransform.mObjMatrix;

		}

	}

}



