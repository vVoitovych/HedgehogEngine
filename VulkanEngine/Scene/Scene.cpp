#include "Scene.hpp"

#include "VulkanEngine/Scene/SceneComponents/TransformComponent.hpp"
#include "VulkanEngine/Scene/SceneComponents/HierarchyComponent.hpp"

#include <sstream>

namespace Scene
{
	void Scene::InitScene()
	{
		mSceneCoordinator.Init();
		//register components
		mSceneCoordinator.RegisterComponent<TransformComponent>();
		mSceneCoordinator.RegisterComponent<HierarchyComponent>();

		//register systems
		mTransformSystem = mSceneCoordinator.RegisterSystem<TransformSystem>();
		mHierarchySystem = mSceneCoordinator.RegisterSystem<HierarchySystem>();

		CreateSceneRoot();

	}

	void Scene::UpdateScene(float dt)
	{
		mTransformSystem->Update(mSceneCoordinator);
		mHierarchySystem->Update(mSceneCoordinator);

	}

	ECS::Entity Scene::CreateGameObject()
	{
		auto& rootHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(mRoot);
		ECS::Entity entity = mSceneCoordinator.CreateEntity();
		mSceneCoordinator.AddComponent(entity, TransformComponent{});
		mSceneCoordinator.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), mRoot, {} });
		rootHierarchy.mChildren.push_back(entity);
		mEntities.insert(entity);

		return entity;
	}

	void Scene::DeleteGameObject(ECS::Entity entity)
	{
		auto& hierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(entity);
		auto& parentHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mParent);
		auto it = std::find(parentHierarchy.mChildren.begin(), parentHierarchy.mChildren.end(), entity);
		parentHierarchy.mChildren.erase(it);
		for (size_t i = 0; i < hierarchy.mChildren.size(); ++i)
		{
			auto& childHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mChildren[i]);
			childHierarchy.mParent = hierarchy.mParent;
			parentHierarchy.mChildren.push_back(hierarchy.mChildren[i]);
		}
		mEntities.erase(entity);
		mSceneCoordinator.DestroyEntity(entity);
	}

	void Scene::CreateSceneRoot()
	{
		mRoot = mSceneCoordinator.CreateEntity();
		mSceneCoordinator.AddComponent(mRoot, TransformComponent{});
		mSceneCoordinator.AddComponent(mRoot, HierarchyComponent{ "Root", mRoot, {} });
		mHierarchySystem->SetRoot(mRoot);

	}

	std::string Scene::GetNewGameObjectName()
	{
		std::stringstream ss;
		ss << "GameObject_" << mGameObjectIndex++;
		return ss.str();
	}


}


