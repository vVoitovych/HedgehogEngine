#include "MeshSystem.hpp"
#include "ContentLoader/CommonFunctions.hpp"
#include "Logger/Logger.hpp"
#include "DialogueWindows/MeshDialogue/MeshDialogue.hpp"

#include <algorithm>
#include <filesystem>

namespace Scene
{
	const std::string MeshSystem::sDefaultMeshPath = "Models\\Default\\cube.obj";

	bool IsFileAccessible(std::string path)
	{
		return std::filesystem::exists(ContentLoader::GetAssetsDirectory() + path);
	}

	void MeshSystem::Update(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
	auto& meshComponent = coordinator.GetComponent<MeshComponent>(entity);
		if (meshComponent.mMeshIndex.has_value())
		{
			size_t index = meshComponent.mMeshIndex.value();
			if (mMeshPathes[index] != meshComponent.mMeshPath)
			{
				CheckMeshPath(meshComponent, meshComponent.mCachedMeshPath);
			}
		}
		else
		{
			CheckMeshPath(meshComponent, sDefaultMeshPath);
		}
	}


	bool MeshSystem::ShouldUpdateMeshContainer() const
	{
		return mUpdateMeshContainer;
	}

	void MeshSystem::MeshContainerUpdated()
	{
		mUpdateMeshContainer = false;
	}

	const std::vector<std::string>& MeshSystem::GetMeshes() const
	{
		return mMeshPathes;
	}

	std::vector<ECS::Entity> MeshSystem::GetEntities()
	{
		return entities;
	}

	void MeshSystem::AddMeshPath(std::string meshPath)
	{
		mMeshPathes.push_back(meshPath);
	}

	void MeshSystem::LoadMesh(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		char* path = DialogueWindows::MeshOpenDialogue();
		if (path == nullptr)
		{
			return;
		}

		std::string relatedPath = ContentLoader::GetAssetRelativetlyPath(path);
		auto& meshComponent = coordinator.GetComponent<MeshComponent>(entity);
		meshComponent.mMeshPath = relatedPath;
	}

	void MeshSystem::CheckMeshPath(MeshComponent& meshComponent, std::string fallbackPath)
	{
		auto it = std::find(mMeshPathes.begin(), mMeshPathes.end(), meshComponent.mMeshPath);
		if (it != mMeshPathes.end())
		{
			size_t newIndex = it - mMeshPathes.begin();
			meshComponent.mMeshIndex = newIndex;
			meshComponent.mCachedMeshPath = meshComponent.mMeshPath;
		}
		else
		{
			if (IsFileAccessible(meshComponent.mMeshPath))
			{
				meshComponent.mCachedMeshPath = meshComponent.mMeshPath;
				meshComponent.mMeshIndex = mMeshPathes.size();
				mMeshPathes.push_back(meshComponent.mMeshPath);
				mUpdateMeshContainer = true;
			}
			else
			{
				LOGERROR("Wrong file path: ", meshComponent.mMeshPath);
				meshComponent.mMeshPath = fallbackPath;
			}
		}
	}

}



