#pragma once

#include "ECS/Coordinator.h"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"

#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
	class HierarchyComponent;

	struct RenderObjectData
	{
		glm::mat4 objMatrix;
		size_t meshIndex;
	};

	class Scene
	{

	public:
		void InitScene();
		void UpdateScene(float dt);

		ECS::Entity CreateGameObject();
		ECS::Entity CreateGameObject(ECS::Entity entity);
		void DeleteGameObject(ECS::Entity parentEntity);

		void AddMeshComponent(ECS::Entity& entity);
		void AddMeshComponent(ECS::Entity& entity, std::string mesh);
		void RemoveMeshComponent(ECS::Entity& entity);
		void ChangeMeshComponent(ECS::Entity& entity, std::string meshPath);

		std::vector<RenderObjectData> GetRenderGameObjects();

		ECS::Entity GetRoot() const;
		HierarchyComponent& GetHierarchyComponent(ECS::Entity entity);

	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();

	private:
		ECS::Coordinator mSceneCoordinator;
		ECS::Entity mRoot;

		// systems
		std::shared_ptr<TransformSystem> mTransformSystem;
		std::shared_ptr<HierarchySystem> mHierarchySystem;
		std::shared_ptr<MeshSystem> mMeshSystem;

		uint16_t mGameObjectIndex = 0;
	};
}









