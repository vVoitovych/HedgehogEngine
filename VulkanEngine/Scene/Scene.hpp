#pragma once

#include "ECS/Coordinator.h"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"

#include <optional>
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

		std::string GetSceneName() const;

		ECS::Entity CreateGameObject();
		void DeleteGameObject();

		void AddMeshComponent(ECS::Entity& entity);
		void AddMeshComponent(ECS::Entity& entity, std::string mesh);
		void RemoveMeshComponent(ECS::Entity& entity);
		void ChangeMeshComponent(ECS::Entity& entity, std::string meshPath);

		std::vector<RenderObjectData> GetRenderGameObjects();

		ECS::Entity GetRoot() const;
		HierarchyComponent& GetHierarchyComponent(ECS::Entity entity);
		TransformComponent& GetTransformComponent(ECS::Entity entity);

		bool IsGameObjectSelected() const;
		ECS::Entity GetSelectedGameObject() const;
		void UnselectGameObject();
		void SelectGameObject(ECS::Entity entity);

	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();

	private:
		std::string mSceneName;

		ECS::Coordinator mSceneCoordinator;
		ECS::Entity mRoot;

		std::optional<ECS::Entity> mSelectedEntity;

		// systems
		std::shared_ptr<TransformSystem> mTransformSystem;
		std::shared_ptr<HierarchySystem> mHierarchySystem;
		std::shared_ptr<MeshSystem> mMeshSystem;

		uint16_t mGameObjectIndex = 0;
	};
}









