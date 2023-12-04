#pragma once

#include "VulkanEngine/ECS/Coordinator.h"
#include "VulkanEngine/Scene/SceneSystems/TransformSystem.hpp"
#include "VulkanEngine/Scene/SceneSystems/HierarchySystem.hpp"

#include <set>

namespace Scene
{
	class Scene
	{
	public:
		void InitScene();
		void UpdateScene(float dt);

		ECS::Entity CreateGameObject();
		void DeleteGameObject(ECS::Entity entity);

	private:
		void CreateSceneRoot();
		std::string GetNewGameObjectName();

	private:
		ECS::Coordinator mSceneCoordinator;
		std::set<ECS::Entity> mEntities;
		ECS::Entity mRoot;

		// systems
		std::shared_ptr<TransformSystem> mTransformSystem;
		std::shared_ptr<HierarchySystem> mHierarchySystem;


		uint16_t mGameObjectIndex = 0;
	};
}









