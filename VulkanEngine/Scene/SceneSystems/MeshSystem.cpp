#include "MeshSystem.hpp"
#include "VulkanEngine/ContentLoader/CommonFunctions.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

#include <algorithm>
#include <filesystem>

namespace Scene
{
	const std::string MeshSystem::sDefaultMeshPath = "Models\\Default\\cube.obj";

	bool IsFileAccessible(std::string path)
	{
		return std::filesystem::exists(ContentLoader::GetAssetsDirectory() + path);
	}

	void MeshSystem::Update(ECS::Coordinator& coordinator)
	{
		for (auto const& entity : entities)
		{
			auto& meshComponent = coordinator.GetComponent<MeshComponent>(entity);
			if (meshComponent.mMeshIndex.has_value())
			{
				// already initialized
				size_t index = meshComponent.mMeshIndex.value();
				if (mMeshPathes[index] != meshComponent.mMeshPath)
				{
					CheckMeshPath(meshComponent, meshComponent.mCachedMeshPath);
				}
			}
			else
			{
				// should be initialize
				CheckMeshPath(meshComponent, sDefaultMeshPath);
			}
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

	std::vector<std::string> MeshSystem::GetMeshes()
	{
		return mMeshPathes;
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



