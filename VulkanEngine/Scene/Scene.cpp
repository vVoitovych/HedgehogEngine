#include "Scene.hpp"
#include "SceneSerializer.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#include <sstream>

namespace Scene
{
	Scene::Scene()
	{
		mSceneName = "Default";
	}

	Scene::~Scene()
	{
	}

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


		mMeshes.push_back("Models\\viking_room.obj");
		mTextures.push_back("Textures\\viking_room.png");
	}

	void Scene::UpdateScene(float dt)
	{
		mTransformSystem->Update(mSceneCoordinator);
		mHierarchySystem->Update(mSceneCoordinator);
		mMeshSystem->Update(mSceneCoordinator);

	}

	void Scene::ResetScene()
	{
		while (true)
		{
			auto hierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(mRoot);
			if (hierarchy.mChildren.size() > 0)
			{
				mSelectedEntity = hierarchy.mChildren[0];
				DeleteGameObject();
				UnselectGameObject();
			}
			else
			{
				return;
			}
		}
	}

	void Scene::Load()
	{
		SceneSerializer::DeserializeScene(*this, GetScenePath());
	}

	void Scene::Save()
	{
		SceneSerializer::SerializeScene(*this, GetScenePath());
	}

	std::string Scene::GetSceneName() const
	{
		return mSceneName;
	}

	void Scene::SetSceneName(std::string& str)
	{
		mSceneName = str;
	}

	ECS::Entity Scene::CreateGameObject()
	{
		ECS::Entity parentEntity;
		if (IsGameObjectSelected())
		{
			parentEntity = mSelectedEntity.value();
		}
		else
		{
			parentEntity = mRoot;
		}
		auto& rootHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(parentEntity);
		ECS::Entity entity = mSceneCoordinator.CreateEntity();
		mSceneCoordinator.AddComponent(entity, TransformComponent{});
		mSceneCoordinator.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), parentEntity, {} });
		rootHierarchy.mChildren.push_back(entity);

		return entity;
	}

	void Scene::DeleteGameObject()
	{	
		ECS::Entity entity;
		if (IsGameObjectSelected())
		{
			entity = mSelectedEntity.value();
			auto& hierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(entity);
			auto& parentHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mParent);
			auto it = std::find(parentHierarchy.mChildren.begin(), parentHierarchy.mChildren.end(), entity);
			if (it != parentHierarchy.mChildren.end())
			{
				parentHierarchy.mChildren.erase(it);
				for (size_t i = 0; i < hierarchy.mChildren.size(); ++i)
				{
					auto& childHierarchy = mSceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mChildren[i]);
					childHierarchy.mParent = hierarchy.mParent;
					parentHierarchy.mChildren.push_back(hierarchy.mChildren[i]);
				}
			}
			mSceneCoordinator.DestroyEntity(entity);

			UnselectGameObject();
		}

	}

	void Scene::AddMeshComponent()
	{
		ECS::Entity entity;
		if (IsGameObjectSelected())
		{
			entity = mSelectedEntity.value();
			mSceneCoordinator.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
		}
	}

	void Scene::AddMeshComponent(std::string mesh)
	{
		ECS::Entity entity;
		if (IsGameObjectSelected())
		{
			entity = mSelectedEntity.value();
			mSceneCoordinator.AddComponent(entity, MeshComponent{ mesh });
		}
	}

	void Scene::RemoveMeshComponent()
	{
		ECS::Entity entity;
		if (IsGameObjectSelected())
		{
			entity = mSelectedEntity.value();
			mSceneCoordinator.RemoveComponent<MeshComponent>(entity);
		}
	}

	void Scene::ChangeMeshComponent(std::string meshPath)
	{
		ECS::Entity entity;
		if (IsGameObjectSelected())
		{
			entity = mSelectedEntity.value();
			auto& meshComponent = mSceneCoordinator.GetComponent<MeshComponent>(entity);
			meshComponent.mMeshPath = meshPath;
		}
	}

	bool Scene::HasMeshComponent(ECS::Entity& entity) const
	{
		return mSceneCoordinator.HasComponent<MeshComponent>(entity);
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

	const std::vector<std::string>& Scene::GetMeshes() const
	{
		return mMeshes;
	}

	const std::vector<std::string>& Scene::GetTextures() const
	{
		return mTextures;
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

	std::string Scene::GetScenePath() const
	{
		std::string result = ContentLoader::GetAssetsDirectory() + "Scenes\\" + mSceneName;

		return result;
	}


}


