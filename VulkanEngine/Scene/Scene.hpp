#pragma once

#include "ECS/Coordinator.h"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"

#include <optional>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
	class HierarchyComponent;
	class TransformComponent;

	struct RenderObjectData
	{
		glm::mat4 objMatrix;
		size_t meshIndex;
	};

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


		std::vector<RenderObjectData> GetRenderGameObjects();

		ECS::Entity GetRoot() const;
		HierarchyComponent& GetHierarchyComponent(ECS::Entity entity);
		TransformComponent& GetTransformComponent(ECS::Entity entity);
		MeshComponent& GetMeshComponent(ECS::Entity entity);

		bool IsGameObjectSelected() const;
		ECS::Entity GetSelectedGameObject() const;
		void UnselectGameObject();
		void SelectGameObject(ECS::Entity entity);

		const std::vector<std::string>& GetMeshes() const;
		const std::vector<std::string>& GetTextures() const;

	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();
		std::string GetScenePath() const;

	private:
		std::string mSceneName;

		std::vector<std::string> mMeshes;
		std::vector<std::string> mTextures;

		ECS::Coordinator mSceneCoordinator;
		ECS::Entity mRoot;

		std::optional<ECS::Entity> mSelectedEntity;

		// systems
		std::shared_ptr<TransformSystem> mTransformSystem;
		std::shared_ptr<HierarchySystem> mHierarchySystem;
		std::shared_ptr<MeshSystem> mMeshSystem;

		uint16_t mGameObjectIndex = 0;
	private:
		friend class SceneSerializer;
	};
}









