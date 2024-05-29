#include "RenderSystem.hpp"

#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#include "Logger/Logger.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "ThirdParty/yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace Scene
{
	void RenderSystem::Update(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		auto& render = coordinator.GetComponent<RenderComponent>(entity);
		if (render.mMaterialIndex.has_value())
		{
			if (mMaterialPathes[render.mMaterialIndex.value()] != render.mMaterial)
			{
				UpdateMaterialPath(coordinator, entity);
			}
		}
		else
		{
			UpdateMaterialPath(coordinator, entity);
		}
	}

	void RenderSystem::CreateMaterial()
    {
		char* path = DialogueWindows::MaterialCreationDialogue();
		if (path == nullptr)
		{
			return;
		}
		std::string relatedPath = ContentLoader::GetAssetRelativetlyPath(path);

		LOGINFO("Material created: ", path);
		
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material path" << YAML::Value << relatedPath;

		out << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();
    }

	void RenderSystem::LoadMaterial(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		char* path = DialogueWindows::MaterialOpenDialogue();
		if (path == nullptr)
		{
			return;
		}
		std::string relatedPath = ContentLoader::GetAssetRelativetlyPath(path);
		auto& component = coordinator.GetComponent<RenderComponent>(entity);
		component.mMaterial = relatedPath;

		UpdateMaterialPath(coordinator, entity);
	}

	const std::vector<std::string>& RenderSystem::GetMaterials() const
	{
		return mMaterialPathes;
	}

	void RenderSystem::UpdateMaterialPath(ECS::Coordinator& coordinator, ECS::Entity entity)
	{
		auto& component = coordinator.GetComponent<RenderComponent>(entity);

		if (!component.mMaterial.empty())
		{
			auto it = std::find(mMaterialPathes.begin(), mMaterialPathes.end(), component.mMaterial);
			if (it != mMaterialPathes.end())
			{
				component.mMaterialIndex = it - mMaterialPathes.begin();
			}
			else
			{
				component.mMaterialIndex = mMaterialPathes.size();
				mMaterialPathes.push_back(component.mMaterial);
			}

		}
	}

}



