#include "MaterialSerializer.hpp"

#include "MaterialData.hpp"
#include "Logger/Logger.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "ThirdParty/yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace Renderer
{
	void MaterialSerializer::Serialize(MaterialData& material)
	{
		LOGINFO("Serialize material: ", material.path);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Type" << YAML::Value << static_cast<size_t>(material.type);
		out << YAML::Key << "BaseColor" << YAML::Value << material.baseColor;
		out << YAML::Key << "Transparency" << YAML::Value << material.tansparency;
		out << YAML::EndMap;

		std::ofstream fout(material.path);
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
		material.path = materialPath;
		material.type =	static_cast<MaterialType>(data["Type"].as<size_t>());
		material.baseColor = data["BaseColor"].as<std::string>();
		material.tansparency = data["Transparency"].as<float>();;
	}

}





