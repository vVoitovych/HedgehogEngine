#include "Scene.hpp"
#include "SceneSerializer.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "ContentLoader/CommonFunctions.hpp"
#include "Logger/Logger.hpp"
#include "DialogueWindows/SceneDialogue/SceneDialogue.hpp"
#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"

#include <sstream>
#include <fstream>
#include <filesystem>

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
		mSceneCoordinator.RegisterComponent<RenderComponent>();

		//register systems
		mTransformSystem = mSceneCoordinator.RegisterSystem<TransformSystem>();
		mHierarchySystem = mSceneCoordinator.RegisterSystem<HierarchySystem>();
		mMeshSystem = mSceneCoordinator.RegisterSystem<MeshSystem>();
		mLightSystem = mSceneCoordinator.RegisterSystem<LightSystem>();
		mRenderSystem = mSceneCoordinator.RegisterSystem<RenderSystem>();

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
		signature.reset();
		signature.set(mSceneCoordinator.GetComponentType<RenderComponent>());
		mSceneCoordinator.SetSystemSignature<RenderSystem>(signature);

		// TODO: remove all bellow and add loating instead
		CreateSceneRoot();
		mMeshSystem->AddMeshPath("Models\\viking_room.obj");
		mMeshSystem->AddMeshPath("Models\\Default\\cube.obj");
		mMeshSystem->AddMeshPath("Models\\Default\\sphere.obj");

	}

	void Scene::UpdateScene(float dt)
	{
		mTransformSystem->Update(mSceneCoordinator);
		mHierarchySystem->Update(mSceneCoordinator);
		mLightSystem->Update(mSceneCoordinator);
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

		mMeshSystem->Update(mSceneCoordinator);
		mRenderSystem->UpdataSystem(mSceneCoordinator);
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

			UnselectGameObject();
		}

	}

	void Scene::TryToAddMeshComponent()
	{
		if (mSelectedEntity.has_value())
			AddMeshComponent(mSelectedEntity.value());
	}

	void Scene::AddMeshComponent(ECS::Entity entity)
	{
		if (!HasMeshComponent(entity))
		{
			mSceneCoordinator.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
			mMeshSystem->Update(mSceneCoordinator, entity);
			auto& meshComponent = GetMeshComponent(entity);

			auto& transform = GetTransformComponent(entity);
		}
	}

	void Scene::RemoveMeshComponent()
	{
		if (IsGameObjectSelected() && HasMeshComponent(mSelectedEntity.value()))
		{
			ECS::Entity entity = mSelectedEntity.value();
			mSceneCoordinator.RemoveComponent<MeshComponent>(entity);
		}
	}

	void Scene::ChangeMeshComponent(ECS::Entity entity, std::string meshPath)
	{
		auto& meshComponent = mSceneCoordinator.GetComponent<MeshComponent>(entity);
		meshComponent.mMeshPath = meshPath;
		mMeshSystem->Update(mSceneCoordinator, entity);
	}

	bool Scene::HasMeshComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.HasComponent<MeshComponent>(entity);
	}

	void Scene::LoadMesh(ECS::Entity entity)
	{
		mMeshSystem->LoadMesh(mSceneCoordinator, entity);
	}

	void Scene::TryToAddRenderComponent()
	{
		if (mSelectedEntity.has_value())
		{
			AddRenderComponent(mSelectedEntity.value());
		}
	}

	void Scene::AddRenderComponent(ECS::Entity entity)
	{
		if (!HasRenderComponent(entity))
		{
			mSceneCoordinator.AddComponent(entity, RenderComponent());
			mRenderSystem->Update(mSceneCoordinator, entity);
		}
	}

	void Scene::RemoveRenderComponent()
	{
		if (IsGameObjectSelected() && HasRenderComponent(mSelectedEntity.value()))
		{
			ECS::Entity entity = mSelectedEntity.value();
			mSceneCoordinator.RemoveComponent<RenderComponent>(entity);
		}
	}

	bool Scene::HasRenderComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.HasComponent<RenderComponent>(entity);;
	}

	void Scene::LoadMaterial(ECS::Entity entity)
	{
		char* path = DialogueWindows::MaterialOpenDialogue();
		if (path == nullptr)
		{
			return;
		}
		std::string relatedPath = ContentLoader::GetAssetRelativetlyPath(path);
		auto& component = mSceneCoordinator.GetComponent<RenderComponent>(entity);
		component.mMaterial = relatedPath;		

		mRenderSystem->Update(mSceneCoordinator, entity);
	}

	void Scene::UpdateMaterialComponent(ECS::Entity entity)
	{
		mRenderSystem->Update(mSceneCoordinator, entity);
	}

	void Scene::TryToAddLightComponent()
	{
		if (mSelectedEntity.has_value())
			AddLightComponent(mSelectedEntity.value());
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

	HierarchyComponent& Scene::GetHierarchyComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.GetComponent<HierarchyComponent>(entity);
	}

	TransformComponent& Scene::GetTransformComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.GetComponent<TransformComponent>(entity);
	}

	MeshComponent& Scene::GetMeshComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.GetComponent<MeshComponent>(entity);
	}

	LightComponent& Scene::GetLightComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.GetComponent<LightComponent>(entity);
	}

	RenderComponent& Scene::GetRenderComponent(ECS::Entity entity) const
	{
		return mSceneCoordinator.GetComponent<RenderComponent>(entity);
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

	const std::vector<std::string>& Scene::GetMaterials() const
	{
		return mRenderSystem->GetMaterials();
	}

	const std::vector<ECS::Entity>& Scene::GetRenderableEntities() const
	{
		return mRenderSystem->GetEntities();
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
		static size_t gameObjectIndex = 0;
		std::stringstream ss;
		ss << "GameObject_" << gameObjectIndex++;
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


