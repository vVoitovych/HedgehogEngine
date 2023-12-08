#pragma once

#include "VulkanEngine/ECS/Coordinator.h"
#include "VulkanEngine/ECS/Entity.h"
#include "VulkanEngine/Scene/SceneComponents/MeshComponent.hpp"

#include <vector>

namespace Scene
{
	class MeshSystem : public ECS::System
	{
	public:
		void Update(ECS::Coordinator& coordinator);

		bool ShouldUpdateMeshContainer() const;
		void MeshContainerUpdated();
		std::vector<std::string> GetMeshes();
		std::set<ECS::Entity> GetEntities();

	private:
		void CheckMeshPath(MeshComponent& meshComponent, std::string fallbackPath);

	public:
		static const std::string sDefaultMeshPath;

	private:
		std::vector<std::string> mMeshPathes;

		bool mUpdateMeshContainer = false;


	};
}




