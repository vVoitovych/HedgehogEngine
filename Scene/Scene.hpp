#pragma once

#include "ECS/Coordinator.h"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"
#include "Scene/SceneSystems/LightSystem.hpp"
#include "Scene/SceneSystems/RenderSystem.hpp"
#include "Scene/SceneSystems/ScriptSystem.hpp"

#include <vector>
#include <optional>
#include <string>

namespace Scene
{
	class HierarchyComponent;
	class TransformComponent;

	class Scene
	{

	public:
		Scene();
		~Scene();

		Scene(const Scene&) = delete;
		Scene(Scene&&) = delete;
		Scene& operator=(const Scene&) = delete;
		Scene& operator=(Scene&&) = delete;

		void InitScene();
		void UpdateScene(float dt);
		void ResetScene();
		void Load();
		void Save();
		void RenameScene();

		std::string GetSceneName() const;

		ECS::Entity CreateGameObject(std::optional <ECS::Entity> parentEntity);
		void CreateGameObject(ECS::Entity entity);
		void DeleteGameObject(ECS::Entity entity);
		void DeleteGameObjectAndChildren(ECS::Entity entity);

		void AddMeshComponent(ECS::Entity entity);
		void RemoveMeshComponent(ECS::Entity entity);
		void ChangeMeshComponent(ECS::Entity entity, std::string meshPath);
		bool HasMeshComponent(ECS::Entity entity) const;
		void LoadMesh(ECS::Entity entity);

		void AddRenderComponent(ECS::Entity entity);
		void RemoveRenderComponent(ECS::Entity entity);
		bool HasRenderComponent(ECS::Entity entity) const;
		void LoadMaterial(ECS::Entity entity);
		void UpdateMaterialComponent(ECS::Entity entity);

		void AddScriptComponent(ECS::Entity entity);
		void RemoveScriptComponent(ECS::Entity entity);
		void ChangeScript(ECS::Entity entity, std::string scriptPath);
		bool HasScriptComponent(ECS::Entity entity);

		void AddLightComponent(ECS::Entity entity);
		void RemoveLightComponent(ECS::Entity entity);
		bool HasLightComponent(ECS::Entity entity) const;

		size_t GetLightCount() const;
		const LightComponent& GetLightComponentByIndex(size_t index) const;
		void UpdateShadowCastin(ECS::Entity entity, bool isCast);

		ECS::Entity GetRoot() const;
		HierarchyComponent& GetHierarchyComponent(ECS::Entity entity) const;
		TransformComponent& GetTransformComponent(ECS::Entity entity) const;
		MeshComponent& GetMeshComponent(ECS::Entity entity) const;
		LightComponent& GetLightComponent(ECS::Entity entity) const;
		RenderComponent& GetRenderComponent(ECS::Entity entity) const;

		const std::vector<std::string>& GetMeshes() const;
		const std::vector<std::string>& GetMaterials() const;
		const std::vector<ECS::Entity>& GetRenderableEntities() const;
		const std::optional<HM::Vector3>& GetShadowLightDirection() const;
	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();
		std::string GetScenePath() const;

	private:
		std::string m_SceneName;

		ECS::Coordinator m_SceneCoordinator;
		ECS::Entity m_Root;

		// systems
		std::shared_ptr<TransformSystem> m_TransformSystem;
		std::shared_ptr<HierarchySystem> m_HierarchySystem;
		std::shared_ptr<MeshSystem> m_MeshSystem;
		std::shared_ptr<LightSystem> m_LightSystem;
		std::shared_ptr<RenderSystem> m_RenderSystem;
		std::shared_ptr<ScriptSystem> m_ScriptSystem;
	private:
		friend class SceneSerializer;
	};
}










