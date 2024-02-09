#include "SceneSerializer.hpp"

#include "Scene.hpp"
#include "Logger/Logger.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "ThirdParty/yaml-cpp/yaml.h"

#include <fstream>

namespace Scene
{
	void SceneSerializer::SerializeScene(Scene& scene, std::string scenePath)
	{
		LOGINFO("SerializeScene: ", scenePath, ".yaml");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << scene.GetSceneName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		//m_Scene->m_Registry.each([&](auto entityID)
		//	{
		//		Entity entity = { entityID, m_Scene.get() };
		//		if (!entity)
		//			return;

		//		SerializeEntity(out, entity);
		//	});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(scenePath + ".yaml");
		fout << out.c_str();
	}

	void SceneSerializer::DeserializeScene(Scene& scene, std::string scenePath)
	{
		LOGINFO("DeserializeScene: ", scenePath);
	}
}




