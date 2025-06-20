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
		MeshSystem();
		void Update(ECS::Coordinator& coordinator, ECS::Entity entity);
		void Update(ECS::Coordinator& coordinator);

		bool ShouldUpdateMeshContainer() const;
		void MeshContainerUpdated();
		const std::vector<std::string>& GetMeshes() const; 
		std::vector<ECS::Entity> GetEntities();

		void AddMeshPath(std::string meshPath);

		void LoadMesh(ECS::Coordinator& coordinator, ECS::Entity entity);

	private:
		void CheckMeshPath(MeshComponent& meshComponent, std::string fallbackPath);

	public:
		static const std::string sDefaultMeshPath;

	private:
		std::vector<std::string> mMeshPathes;

		bool mUpdateMeshContainer = false;


	};
}




