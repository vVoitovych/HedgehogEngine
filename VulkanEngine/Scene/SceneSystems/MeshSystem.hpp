#pragma once

#include "ECS/Coordinator.h"
#include "ECS/Entity.h"
#include "Scene/SceneComponents/MeshComponent.hpp"

#include <vector>

namespace Scene
{
	class MeshSystem : public ECS::System
	{
	public:
		void Update(ECS::Coordinator& coordinator, ECS::Entity entity);

		bool ShouldUpdateMeshContainer() const;
		void MeshContainerUpdated();
		const std::vector<std::string>& GetMeshes() const; 
		std::set<ECS::Entity> GetEntities();

		void AddMeshPath(std::string meshPath);
	private:
		void CheckMeshPath(MeshComponent& meshComponent, std::string fallbackPath);

	public:
		static const std::string sDefaultMeshPath;

	private:
		std::vector<std::string> mMeshPathes;

		bool mUpdateMeshContainer = false;


	};
}




