#include "SceneSerializer.hpp"
#include "Scene.hpp"
#include "SceneComponents/TransformComponent.hpp"
#include "SceneComponents/HierarchyComponent.hpp"

#include "Logger/Logger.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "ThirdParty/yaml-cpp/yaml.h"

#include <fstream>

namespace Scene
{

	static YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	static void SerializeEntity(YAML::Emitter& out, Scene& scene, ECS::Entity entity)
	{
		auto& hierarchy = scene.GetHierarchyComponent(entity);
		auto& transform = scene.GetTransformComponent(entity);
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity;
		out << YAML::Key << "Name" << YAML::Value << hierarchy.mName;
		out << YAML::Key << "Parent" << YAML::Value << hierarchy.mParent;
		{ // transform
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			out << YAML::Key << "Position" << YAML::Value << transform.mPososition;
			out << YAML::Key << "Rotation" << YAML::Value << transform.mRotation;
			out << YAML::Key << "Scale" << YAML::Value << transform.mScale;

			out << YAML::EndMap; // TransformComponent
		}
		out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
		for (auto child : hierarchy.mChildren)
		{
			SerializeEntity(out, scene, child);
		}
		out << YAML::EndSeq;
		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::SerializeScene(Scene& scene, std::string scenePath)
	{
		LOGINFO("SerializeScene: ", scenePath, ".yaml");
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene name" << YAML::Value << scene.GetSceneName();
		out << YAML::Key << "Scene" << YAML::Value << YAML::BeginSeq;
		
		SerializeEntity(out, scene, scene.GetRoot());

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




