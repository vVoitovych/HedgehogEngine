#include "Scene.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"

#include <sstream>

namespace Scene
{
	void Scene::InitScene()
	{
		mSceneCoordinator.Init();
		//register components
		mSceneCoordinator.RegisterComponent<TransformComponent>();
		mSceneCoordinator.RegisterComponent<HierarchyComponent>();
		mSceneCoordinator.RegisterComponent<MeshComponent>();

		//register systems
		mTransformSystem = mSceneCoordinator.RegisterSystem<TransformSystem>();
		mHierarchySystem = mSceneCoordinator.RegisterSystem<HierarchySystem>();
		mMeshSystem = mSceneCoordinator.RegisterSystem<MeshSystem>();

		//bind systems and components
		ECS::Signature signature;
		signature.set(mSceneCoordinator.GetComponentType<TransformComponent>());
		mSceneCoordinator.SetSystemSignature<TransformSystem>(signature);
		signature.set(mSceneCoordinator.GetComponentType<HierarchyComponent>());
		mSceneCoordinator.SetSystemSignature<HierarchySystem>(signature);
		signature.set(mSceneCoordinator.GetComponentType<MeshComponent>());
		mSceneCoordinator.SetSystemSignature<MeshSystem>(signature);

		CreateSceneRoot();

	}

	void Scene::UpdateScene(float dt)
	{
		mTransformSystem->Update(mSceneCoordinator);
		mHierarchySystem->Update(mSceneCoordinator);
		mMeshSystem->Update(mSceneCoordinator);

	}

	ECS::Entity Scene::CreateGameObject()
	{
		auto& rootHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(mRoot);
		ECS::Entity entity = mSceneCoordinator.CreateEntity();
		mSceneCoordinator.AddComponent(entity, TransformComponent{});
		mSceneCoordinator.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), mRoot, {} });
		rootHierarchy.mChildren.push_back(entity);

		return entity;
	}

	ECS::Entity Scene::CreateGameObject(ECS::Entity parentEntity)
	{
		auto& rootHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(parentEntity);
		ECS::Entity entity = mSceneCoordinator.CreateEntity();
		mSceneCoordinator.AddComponent(entity, TransformComponent{});
		mSceneCoordinator.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), parentEntity, {} });
		rootHierarchy.mChildren.push_back(entity);

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
		mSceneCoordinator.DestroyEntity(entity);
	}

	void Scene::AddMeshComponent(ECS::Entity& entity)
	{
		mSceneCoordinator.AddComponent(entity, MeshComponent{MeshSystem::sDefaultMeshPath});
	}

	void Scene::AddMeshComponent(ECS::Entity& entity, std::string mesh)
	{
		mSceneCoordinator.AddComponent(entity, MeshComponent{ mesh });
	}

	void Scene::RemoveMeshComponent(ECS::Entity& entity)
	{
		mSceneCoordinator.RemoveComponent<MeshComponent>(entity);
	}

	void Scene::ChangeMeshComponent(ECS::Entity& entity, std::string meshPath)
	{
		auto& meshComponent = mSceneCoordinator.GetComponent<MeshComponent>(entity);
		meshComponent.mMeshPath = meshPath;
	}

	std::vector<RenderObjectData> Scene::GetRenderGameObjects()
	{
		std::vector<RenderObjectData> result;
		result.clear();



		return result;
	}

	ECS::Entity Scene::GetRoot() const
	{
		return mRoot;
	}

	HierarchyComponent& Scene::GetHierarchyComponent(ECS::Entity entity)
	{
		return mSceneCoordinator.GetComponent<HierarchyComponent>(entity);
	}

	TransformComponent& Scene::GetTransformComponent(ECS::Entity entity)
	{
		return mSceneCoordinator.GetComponent<TransformComponent>(entity);
	}

	bool Scene::IsGameObjectSelected() const
	{
		return mSelectedEntity.has_value();
	}

	ECS::Entity Scene::GetSelectedGameObject() const
	{
		if (!mSelectedEntity.has_value())
		{
			throw std::runtime_error("No game object selected");
		}
		return mSelectedEntity.value();
	}

	void Scene::UnselectGameObject()
	{
		mSelectedEntity.reset();
	}

	void Scene::SelectGameObject(ECS::Entity entity)
	{
		if (entity != mRoot)
			mSelectedEntity = entity;
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


