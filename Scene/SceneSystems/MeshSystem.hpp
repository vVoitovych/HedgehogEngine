#pragma once

#include "ECS/ECS.h"
#include "ECS/Entity.h"
#include "Scene/SceneComponents/MeshComponent.hpp"

#include <vector>

namespace Scene
{
	class MeshSystem : public ECS::System
	{
	public:
		MeshSystem();
		void Update(ECS::ECS& ecs, ECS::Entity entity);
		void Update(ECS::ECS& ecs);

		bool ShouldUpdateMeshContainer() const;
		void MeshContainerUpdated();
		const std::vector<std::string>& GetMeshes() const;
		std::vector<ECS::Entity> GetEntities();

		void AddMeshPath(std::string meshPath);

		void LoadMesh(ECS::ECS& ecs, ECS::Entity entity);

	private:
		void CheckMeshPath(MeshComponent& meshComponent, std::string fallbackPath);

	public:
		static const std::string sDefaultMeshPath;

	private:
		std::vector<std::string> mMeshPathes;

		bool mUpdateMeshContainer = false;


	};
}




