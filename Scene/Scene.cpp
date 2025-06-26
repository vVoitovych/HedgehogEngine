#include "Scene.hpp"
#include "SceneSerializer.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"
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
		m_SceneName = "Default";
		m_Root = 0;
	}

	Scene::~Scene()
	{
	}

	void Scene::InitScene()
	{
		m_SceneCoordinator.Init();
		//register components
		m_SceneCoordinator.RegisterComponent<TransformComponent>();
		m_SceneCoordinator.RegisterComponent<HierarchyComponent>();
		m_SceneCoordinator.RegisterComponent<MeshComponent>();
		m_SceneCoordinator.RegisterComponent<LightComponent>();
		m_SceneCoordinator.RegisterComponent<RenderComponent>();
		m_SceneCoordinator.RegisterComponent<ScriptComponent>();

		//register systems
		m_TransformSystem = m_SceneCoordinator.RegisterSystem<TransformSystem>();
		m_HierarchySystem = m_SceneCoordinator.RegisterSystem<HierarchySystem>();
		m_MeshSystem = m_SceneCoordinator.RegisterSystem<MeshSystem>();
		m_LightSystem = m_SceneCoordinator.RegisterSystem<LightSystem>();
		m_RenderSystem = m_SceneCoordinator.RegisterSystem<RenderSystem>();
		m_ScriptSystem = m_SceneCoordinator.RegisterSystem<ScriptSystem>();

		//bind systems and components
		ECS::Signature signature;
		signature.set(m_SceneCoordinator.GetComponentType<TransformComponent>());
		m_SceneCoordinator.SetSystemSignature<TransformSystem>(signature);
		signature.reset();
		signature.set(m_SceneCoordinator.GetComponentType<HierarchyComponent>());
		m_SceneCoordinator.SetSystemSignature<HierarchySystem>(signature);
		signature.reset();
		signature.set(m_SceneCoordinator.GetComponentType<MeshComponent>());
		m_SceneCoordinator.SetSystemSignature<MeshSystem>(signature);
		signature.reset();
		signature.set(m_SceneCoordinator.GetComponentType<LightComponent>());
		m_SceneCoordinator.SetSystemSignature<LightSystem>(signature);
		signature.reset();
		signature.set(m_SceneCoordinator.GetComponentType<RenderComponent>());
		m_SceneCoordinator.SetSystemSignature<RenderSystem>(signature);
		signature.reset();
		signature.set(m_SceneCoordinator.GetComponentType<ScriptComponent>());
		m_SceneCoordinator.SetSystemSignature<ScriptSystem>(signature);

		CreateSceneRoot();
	}

	void Scene::UpdateScene(float dt)
	{
		m_TransformSystem->Update(m_SceneCoordinator);
		m_HierarchySystem->Update(m_SceneCoordinator);
		m_LightSystem->Update(m_SceneCoordinator);
	}

	void Scene::ResetScene()
	{
		DeleteGameObjectAndChildren(m_Root);
	}

	void Scene::Load()
	{
		char* path = DialogueWindows::SceneOpenDialogue();
		if (path == nullptr)
		{
			return;
		}
		ResetScene();
		SceneSerializer::DeserializeScene(*this, path);

		m_MeshSystem->Update(m_SceneCoordinator);
		m_RenderSystem->UpdataSystem(m_SceneCoordinator);
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
			m_SceneName = newName;
	}

	std::string Scene::GetSceneName() const
	{
		return m_SceneName;
	}

	ECS::Entity Scene::CreateGameObject(std::optional<ECS::Entity> parentEntity)
	{
		ECS::Entity realParentEntity;
		if (parentEntity.has_value())
		{
			realParentEntity = parentEntity.value();
		}
		else
		{
			realParentEntity = m_Root;
		}
		auto& rootHierarchy = m_SceneCoordinator.GetComponent<HierarchyComponent>(realParentEntity);
		ECS::Entity entity = m_SceneCoordinator.CreateEntity();
		m_SceneCoordinator.AddComponent(entity, TransformComponent{});
		m_SceneCoordinator.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), realParentEntity, {} });
		rootHierarchy.mChildren.push_back(entity);

		return entity;
	}

	void Scene::CreateGameObject(ECS::Entity entity)
	{
		m_SceneCoordinator.CreateEntity(entity);
		m_SceneCoordinator.AddComponent(entity, TransformComponent{});
		m_SceneCoordinator.AddComponent(entity, HierarchyComponent{});

	}

	void Scene::DeleteGameObject(ECS::Entity entity)
	{
		auto& hierarchy = m_SceneCoordinator.GetComponent<HierarchyComponent>(entity);
		auto& parentHierarchy = m_SceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mParent);
		auto it = std::find(parentHierarchy.mChildren.begin(), parentHierarchy.mChildren.end(), entity);
		if (it != parentHierarchy.mChildren.end())
		{
			parentHierarchy.mChildren.erase(it);
			for (size_t i = 0; i < hierarchy.mChildren.size(); ++i)
			{
				auto& childHierarchy = m_SceneCoordinator.GetComponent<HierarchyComponent>(hierarchy.mChildren[i]);
				childHierarchy.mParent = hierarchy.mParent;
				parentHierarchy.mChildren.push_back(hierarchy.mChildren[i]);
			}
		}
		m_SceneCoordinator.DestroyEntity(entity);
	}

	void Scene::DeleteGameObjectAndChildren(ECS::Entity entity)
	{
		auto& hierarchy = m_SceneCoordinator.GetComponent<HierarchyComponent>(entity);
		for (size_t i = 0; i < hierarchy.mChildren.size(); ++i)
		{
			DeleteGameObjectAndChildren(hierarchy.mChildren[i]);
		}		
		m_SceneCoordinator.DestroyEntity(entity);
	}


	void Scene::AddMeshComponent(ECS::Entity entity)
	{
		if (!HasMeshComponent(entity))
		{
			m_SceneCoordinator.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
			m_MeshSystem->Update(m_SceneCoordinator, entity);
			auto& meshComponent = GetMeshComponent(entity);

			auto& transform = GetTransformComponent(entity);
		}
	}

	void Scene::RemoveMeshComponent(ECS::Entity entity)
	{
		if (HasMeshComponent(entity))
		{
			m_SceneCoordinator.RemoveComponent<MeshComponent>(entity);
		}
	}

	void Scene::ChangeMeshComponent(ECS::Entity entity, std::string meshPath)
	{
		auto& meshComponent = m_SceneCoordinator.GetComponent<MeshComponent>(entity);
		meshComponent.mMeshPath = meshPath;
		m_MeshSystem->Update(m_SceneCoordinator, entity);
	}

	bool Scene::HasMeshComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.HasComponent<MeshComponent>(entity);
	}

	void Scene::LoadMesh(ECS::Entity entity)
	{
		m_MeshSystem->LoadMesh(m_SceneCoordinator, entity);
	}

	void Scene::AddRenderComponent(ECS::Entity entity)
	{
		if (!HasRenderComponent(entity))
		{
			m_SceneCoordinator.AddComponent(entity, RenderComponent());
			m_RenderSystem->Update(m_SceneCoordinator, entity);
		}
	}

	void Scene::RemoveRenderComponent(ECS::Entity entity)
	{
		if (HasRenderComponent(entity))
		{
			m_SceneCoordinator.RemoveComponent<RenderComponent>(entity);
		}
	}

	bool Scene::HasRenderComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.HasComponent<RenderComponent>(entity);;
	}

	void Scene::LoadMaterial(ECS::Entity entity)
	{
		char* path = DialogueWindows::MaterialOpenDialogue();
		if (path == nullptr)
		{
			return;
		}
		std::string relatedPath = ContentLoader::GetAssetRelativetlyPath(path);
		auto& component = m_SceneCoordinator.GetComponent<RenderComponent>(entity);
		component.mMaterial = relatedPath;		

		m_RenderSystem->Update(m_SceneCoordinator, entity);
	}

	void Scene::UpdateMaterialComponent(ECS::Entity entity)
	{
		m_RenderSystem->Update(m_SceneCoordinator, entity);
	}

	void Scene::AddScriptComponent(ECS::Entity entity)
	{
		if (!HasScriptComponent(entity))
		{
			m_SceneCoordinator.AddComponent(entity, ScriptComponent());
		}
	}

	void Scene::RemoveScriptComponent(ECS::Entity entity)
	{
		if (HasScriptComponent(entity))
		{
			m_SceneCoordinator.RemoveComponent<ScriptComponent>(entity);
		}
	}

	void Scene::ChangeScript(ECS::Entity entity, std::string scriptPath)
	{
		m_ScriptSystem->ChangeScript(entity, scriptPath, m_SceneCoordinator);
	}

	bool Scene::HasScriptComponent(ECS::Entity entity)
	{
		return m_SceneCoordinator.HasComponent<ScriptComponent>(entity);
	}

	void Scene::AddLightComponent(ECS::Entity entity)
	{
		if (!HasLightComponent(entity))
		{
			m_SceneCoordinator.AddComponent(entity, LightComponent());
		}
	}

	void Scene::RemoveLightComponent(ECS::Entity entity)
	{
		if (HasLightComponent(entity))
		{
			m_SceneCoordinator.RemoveComponent<LightComponent>(entity);
		}
	}

	bool Scene::HasLightComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.HasComponent<LightComponent>(entity);
	}

	size_t Scene::GetLightCount() const
	{
		return m_LightSystem->GetLightComponentsCount();
	}

	const LightComponent& Scene::GetLightComponentByIndex(size_t index) const
	{
		return m_LightSystem->GetLightComponentByIndex(m_SceneCoordinator, index);
	}

	void Scene::UpdateShadowCastin(ECS::Entity entity, bool isCast)
	{
		m_LightSystem->SetShadowCasting(m_SceneCoordinator, entity, isCast);
	}


	ECS::Entity Scene::GetRoot() const
	{
		return m_Root;
	}

	HierarchyComponent& Scene::GetHierarchyComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.GetComponent<HierarchyComponent>(entity);
	}

	TransformComponent& Scene::GetTransformComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.GetComponent<TransformComponent>(entity);
	}

	MeshComponent& Scene::GetMeshComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.GetComponent<MeshComponent>(entity);
	}

	LightComponent& Scene::GetLightComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.GetComponent<LightComponent>(entity);
	}

	RenderComponent& Scene::GetRenderComponent(ECS::Entity entity) const
	{
		return m_SceneCoordinator.GetComponent<RenderComponent>(entity);
	}

	const std::vector<std::string>& Scene::GetMeshes() const 
	{
		return m_MeshSystem->GetMeshes();
	}

	const std::vector<std::string>& Scene::GetMaterials() const
	{
		return m_RenderSystem->GetMaterials();
	}

	const std::vector<ECS::Entity>& Scene::GetRenderableEntities() const
	{
		return m_RenderSystem->GetEntities();
	}

	const std::optional<HM::Vector3>& Scene::GetShadowLightDirection() const
	{
		return m_LightSystem->GetShadowDir();
	}

	void Scene::CreateSceneRoot()
	{
		m_Root = m_SceneCoordinator.CreateEntity();
		m_SceneCoordinator.AddComponent(m_Root, TransformComponent{});
		m_SceneCoordinator.AddComponent(m_Root, HierarchyComponent{ "Root", m_Root, {} });
		m_HierarchySystem->SetRoot(m_Root);

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
		std::string result = ContentLoader::GetAssetsDirectory() + "Scenes\\" + m_SceneName;

		return result;
	}



}


