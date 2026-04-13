#include "HierarchySystem.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

namespace Scene
{
	void HierarchySystem::Update(ECS::ECS& ecs)
	{
		UpdateChildrenMatricies(ecs, mRoot);
	}

	void HierarchySystem::SetRoot(ECS::Entity entity)
	{
		mRoot = entity;
	}

	void HierarchySystem::UpdateChildrenMatricies(ECS::ECS& ecs, ECS::Entity parent)
	{
		auto& transform = ecs.GetComponent<TransformComponent>(parent);
		auto& hierarchy = ecs.GetComponent<HierarchyComponent>(parent);
		for (auto const& entity : hierarchy.mChildren)
		{
			auto& childTransform = ecs.GetComponent<TransformComponent>(entity);
			childTransform.mObjMatrix = transform.mObjMatrix * childTransform.mObjMatrix;
			UpdateChildrenMatricies(ecs, entity);

		}

	}

}



