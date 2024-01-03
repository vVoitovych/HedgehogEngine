#pragma once

#include "VulkanEngine/ECS/Coordinator.h"
#include "VulkanEngine/Scene/SceneSystems/TransformSystem.hpp"
#include "VulkanEngine/Scene/SceneSystems/HierarchySystem.hpp"
#include "VulkanEngine/Scene/SceneSystems/MeshSystem.hpp"

#include <set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
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
		void DeleteGameObject(ECS::Entity entity);

		void AddMeshComponent(ECS::Entity& entity);
		void AddMeshComponent(ECS::Entity& entity, std::string mesh);
		void RemoveMeshComponent(ECS::Entity& entity);
		void ChangeMeshComponent(ECS::Entity& entity, std::string meshPath);

		std::vector<RenderObjectData> GetRenderGameObjects();

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









