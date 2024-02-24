#pragma once

#include "ECS/Coordinator.h"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"
#include "Scene/SceneSystems/LightSystem.hpp"
#include "RenderObjectsManager.hpp"

#include <optional>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

		std::string GetSceneName() const;
		void SetSceneName(std::string& str);

		ECS::Entity CreateGameObject();
		void CreateGameObject(ECS::Entity entity);
		void DeleteGameObject();

		void AddMeshComponent(ECS::Entity entity);
		void RemoveMeshComponent();
		void ChangeMeshComponent(ECS::Entity entity, std::string meshPath);
		bool HasMeshComponent(ECS::Entity entity) const;

		void AddRenderComponent();
		void RemoveRenderComponent();
		bool HasRenderComponent() const;

		void AddLightComponent(ECS::Entity entity);
		void RemoveLightComponent();
		bool HasLightComponent(ECS::Entity entity) const;

		size_t GetLightCount() const;
		const LightComponent& GetLightComponentByIndex(size_t index);

		ECS::Entity GetRoot() const;
		HierarchyComponent& GetHierarchyComponent(ECS::Entity entity);
		TransformComponent& GetTransformComponent(ECS::Entity entity);
		MeshComponent& GetMeshComponent(ECS::Entity entity);
		LightComponent& GetLightComponent(ECS::Entity entity);

		bool IsGameObjectSelected() const;
		ECS::Entity GetSelectedGameObject() const;
		void UnselectGameObject();
		void SelectGameObject(ECS::Entity entity);

		const std::vector<std::string>& GetMeshes() const;
		const std::vector<std::string>& GetTextures() const;
		const std::vector<RenderableObject>& GetRenderableObjects() const;
		void UpdateRendarable(ECS::Entity entity, size_t meshIndex);
	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();
		std::string GetScenePath() const;

	private:
		std::string mSceneName;

		std::vector<std::string> mTextures;

		ECS::Coordinator mSceneCoordinator;
		ECS::Entity mRoot;
		std::optional<ECS::Entity> mSelectedEntity;

		RenderObjectsManager mRenderObjectsManager;
		// systems
		std::shared_ptr<TransformSystem> mTransformSystem;
		std::shared_ptr<HierarchySystem> mHierarchySystem;
		std::shared_ptr<MeshSystem> mMeshSystem;
		std::shared_ptr<LightSystem> mLightSystem;

		uint16_t mGameObjectIndex = 0;
	private:
		friend class SceneSerializer;
	};
}










