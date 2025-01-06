#include "MaterialSerializer.hpp"

#include "MaterialData.hpp"
#include "Logger/Logger.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace Context
{
	void MaterialSerializer::Serialize(MaterialData& material, std::string materialPath)
	{
		LOGINFO("Serialize material: ", materialPath);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(material.type);
		out << YAML::Key << "BaseColor" << YAML::Value << material.baseColor;
		out << YAML::Key << "Transparency" << YAML::Value << material.transparency;
		out << YAML::EndMap;

		std::ofstream fout(materialPath);
		fout << out.c_str();
	}

	void MaterialSerializer::Deserialize(MaterialData& material, std::string materialPath)
	{
		LOGINFO("Deserialize material: ", materialPath);

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(materialPath);
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", materialPath, " with error: ", e.what());
			return;
		}
		material.path = ContentLoader::GetAssetRelativetlyPath(materialPath);
		material.type =	static_cast<MaterialType>(data["Type"].as<size_t>());
		material.baseColor = data["BaseColor"].as<std::string>();
		material.transparency = data["Transparency"].as<float>();;
	}

}





