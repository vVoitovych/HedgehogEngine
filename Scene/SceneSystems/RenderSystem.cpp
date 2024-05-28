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
		if (render.mDirty)
		{
			UpdateMaterialPath(coordinator, entity);
			render.mDirty = false;
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
				
		if (mMaterials.find(path) != mMaterials.end())
		{
			LOGWARNING("Material ", path, " already exist!");
			return;
		}
		mMaterials[path] = CreateDefaultMaterial();
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Material path" << YAML::Value << relatedPath;

		out << YAML::EndMap;

		std::ofstream fout(path);
		fout << out.c_str();

		LOGINFO("Material created: ", path);
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

	const std::unordered_map<std::string, Material>& RenderSystem::GetMaterials() const
	{
		return mMaterials;
	}

	Material RenderSystem::CreateDefaultMaterial()
	{
		Material result;
		const std::string defaulTexture = "Textures\\Default\\white.png";
		
		result.type = MaterialType::Opaque;
		result.baseColor = defaulTexture;
		result.transparency = 1.0f;

		return result;
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



