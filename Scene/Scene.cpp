#include "Scene.hpp"
#include "SceneSerializer.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "ContentLoader/CommonFunctions.hpp"
#include "Logger/Logger.hpp"
#include "DialogueWindows/SceneDialogue/SceneDialogue.hpp"

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
		mSceneCoordinator.RegisterComponent<LightComponent>();

		//register systems
		mTransformSystem = mSceneCoordinator.RegisterSystem<TransformSystem>();
		mHierarchySystem = mSceneCoordinator.RegisterSystem<HierarchySystem>();
		mMeshSystem = mSceneCoordinator.RegisterSystem<MeshSystem>();
		mLightSystem = mSceneCoordinator.RegisterSystem<LightSystem>();

		//bind systems and components
		ECS::Signature signature;
		signature.set(mSceneCoordinator.GetComponentType<TransformComponent>());
		mSceneCoordinator.SetSystemSignature<TransformSystem>(signature);
		signature.reset();
		signature.set(mSceneCoordinator.GetComponentType<HierarchyComponent>());
		mSceneCoordinator.SetSystemSignature<HierarchySystem>(signature);
		signature.reset();
		signature.set(mSceneCoordinator.GetComponentType<MeshComponent>());
		mSceneCoordinator.SetSystemSignature<MeshSystem>(signature);
		signature.reset();
		signature.set(mSceneCoordinator.GetComponentType<LightComponent>());
		mSceneCoordinator.SetSystemSignature<LightSystem>(signature);


		// TODO: remove all bellow and add loating instead
		CreateSceneRoot();
		mMeshSystem->AddMeshPath("Models\\viking_room.obj");
		mMeshSystem->AddMeshPath("Models\\Default\\cube.obj");
		mMeshSystem->AddMeshPath("Models\\Default\\sphere.obj");

		mTextures.push_back("Textures\\viking_room.png");

	}

	void Scene::UpdateScene(float dt)
	{
		mTransformSystem->Update(mSceneCoordinator);
		mHierarchySystem->Update(mSceneCoordinator);
		mLightSystem->UpdateLights(mSceneCoordinator);
		auto& objects = mRenderObjectsManager.GetRenderableObjects();
		for (size_t i = 0; i < objects.size(); ++i)
		{
			auto entity = mRenderObjectsManager.GetEntityByIndex(i);
			auto& transform = mSceneCoordinator.GetComponent<TransformComponent>(entity);
			objects[i].objMatrix = transform.mObjMatrix;
		}
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
		char* path = DialogueWindows::SceneOpenDialogue();
		if (path == nullptr)
		{
			return;
		}
		UnselectGameObject();
		ResetScene();
		mSceneCoordinator.DestroyEntity(mRoot);
		SceneSerializer::DeserializeScene(*this, path);

	}

	void Scene::Save()
	{
		char* path = DialogueWindows::SceneSaveDialogue();
		if (path == nullptr)
		{
			return;
		}

		SceneSerializer::SerializeScene(*this, path);
	}

	void Scene::RenameScene()
	{
		char* newName = DialogueWindows::SceneRenameDialogue();
		if (newName != nullptr)
			mSceneName = newName;
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
			mRenderObjectsManager.RemoveEntity(entity);

			UnselectGameObject();
		}

	}

	void Scene::AddMeshComponent(ECS::Entity entity)
	{
		if (!HasMeshComponent(entity))
		{
			mSceneCoordinator.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
			mMeshSystem->Update(mSceneCoordinator, entity);
			auto& meshComponent = GetMeshComponent(entity);

			auto& transform = GetTransformComponent(entity);
			auto& object = mRenderObjectsManager.AddEntity(entity);
			object.isVisible = true;
			object.meshIndex = meshComponent.mMeshIndex.value();
			object.objMatrix = transform.mObjMatrix;
		}
	}

	void Scene::RemoveMeshComponent()
	{
		if (IsGameObjectSelected() && HasMeshComponent(mSelectedEntity.value()))
		{
			ECS::Entity entity = mSelectedEntity.value();
			mSceneCoordinator.RemoveComponent<MeshComponent>(entity);

			mRenderObjectsManager.RemoveEntity(entity);
		}
	}

	void Scene::ChangeMeshComponent(ECS::Entity entity, std::string meshPath)
	{
		auto& meshComponent = mSceneCoordinator.GetComponent<MeshComponent>(entity);
		meshComponent.mMeshPath = meshPath;
		mMeshSystem->Update(mSceneCoordinator, entity);

		auto& object = mRenderObjectsManager.GetEntityData(entity);
		object.meshIndex = meshComponent.mMeshIndex.value();
	}

	bool Scene::HasMeshComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.HasComponent<MeshComponent>(entity);
	}

	void Scene::AddRenderComponent()
	{
	}

	void Scene::RemoveRenderComponent()
	{
	}

	bool Scene::HasRenderComponent() const
	{
		return false;
	}

	void Scene::AddLightComponent(ECS::Entity entity)
	{
		if (!HasLightComponent(entity))
		{
			mSceneCoordinator.AddComponent(entity, LightComponent());
		}
	}

	void Scene::RemoveLightComponent()
	{
		if (IsGameObjectSelected() && HasLightComponent(mSelectedEntity.value()))
		{
			ECS::Entity entity = mSelectedEntity.value();
			mSceneCoordinator.RemoveComponent<LightComponent>(entity);
		}
	}

	bool Scene::HasLightComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.HasComponent<LightComponent>(entity);
	}

	size_t Scene::GetLightCount() const
	{
		return mLightSystem->GetLightComponentsCount();
	}

	const LightComponent& Scene::GetLightComponentByIndex(size_t index) const
	{
		return mLightSystem->GetLightComponentByIndex(mSceneCoordinator, index);
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

	MeshComponent& Scene::GetMeshComponent(ECS::Entity entity)
	{
		return mSceneCoordinator.GetComponent<MeshComponent>(entity);
	}

	LightComponent& Scene::GetLightComponent(ECS::Entity entity)
	{
		return mSceneCoordinator.GetComponent<LightComponent>(entity);
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
		{
			mSelectedEntity = entity;
		}
		else
		{
			UnselectGameObject();
		}
	}

	const std::vector<std::string>& Scene::GetMeshes() const 
	{
		return mMeshSystem->GetMeshes();
	}

	const std::vector<std::string>& Scene::GetTextures() const
	{
		return mTextures;
	}

	const std::vector<RenderableObject>& Scene::GetRenderableObjects() const
	{
		return mRenderObjectsManager.GetRenderableObjects();
	}

	void Scene::UpdateRendarable(ECS::Entity entity, size_t meshIndex)
	{
		auto& meshComponent = GetMeshComponent(entity);
		auto& transform = GetTransformComponent(entity);
		auto& object = mRenderObjectsManager.GetEntityData(entity);
		object.isVisible = true;
		object.meshIndex = meshComponent.mMeshIndex.value();
		object.objMatrix = transform.mObjMatrix;
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

	void Scene::CreateGameObject(ECS::Entity entity)
	{
		mSceneCoordinator.CreateEntity(entity);
		mSceneCoordinator.AddComponent(entity, TransformComponent{});
		mSceneCoordinator.AddComponent(entity, HierarchyComponent{});

	}


}


