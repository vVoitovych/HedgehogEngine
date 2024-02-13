#include "SceneSerializer.hpp"
#include "Scene.hpp"
#include "SceneComponents/TransformComponent.hpp"
#include "SceneComponents/HierarchyComponent.hpp"

#include "Logger/Logger.hpp"


#define YAML_CPP_STATIC_DEFINE
#include "ThirdParty/yaml-cpp/yaml.h"

#include <fstream>


namespace YAML {

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
}

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
			out << YAML::BeginMap; 

			out << YAML::Key << "Position" << YAML::Value << transform.mPososition;
			out << YAML::Key << "Rotation" << YAML::Value << transform.mRotation;
			out << YAML::Key << "Scale" << YAML::Value << transform.mScale;

			out << YAML::EndMap; 
		}
		if (scene.HasMeshComponent(entity))
		{ // mesh component
			auto& mesh = scene.GetMeshComponent(entity);
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; 

			out << YAML::Key << "MeshPath" << YAML::Value << mesh.mMeshPath;
			out << YAML::Key << "MeshIndex" << YAML::Value << mesh.mMeshIndex.value();
			out << YAML::Key << "CachedMeshPath" << YAML::Value << mesh.mCachedMeshPath;

			out << YAML::EndMap; 
		}
		// annd other components here
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

	static void DeserializeEntity(Scene& scene, YAML::Node node)
	{
		ECS::Entity entity = node["Entity"].as<ECS::Entity>();
		scene.CreateGameObject(entity);
		auto& transform = scene.GetTransformComponent(entity);
		auto& hierarchy = scene.GetHierarchyComponent(entity);

		auto transformData = node["TransformComponent"];
		if (transformData)
		{
			transform.mPososition = transformData["Position"].as<glm::vec3>();
			transform.mRotation = transformData["Rotation"].as<glm::vec3>();
			transform.mScale = transformData["Scale"].as<glm::vec3>();
		}

		auto mesh = node["MeshComponent"];
		if (mesh)
		{
			scene.AddMeshComponent(entity);
			auto& meshComponent = scene.GetMeshComponent(entity);
			meshComponent.mMeshPath = mesh["MeshPath"].as<std::string>();
			meshComponent.mMeshIndex = mesh["MeshIndex"].as<size_t>();
			meshComponent.mCachedMeshPath = mesh["CachedMeshPath"].as<std::string>();
		}

		hierarchy.mName = node["Name"].as<std::string>();
		hierarchy.mParent = node["Parent"].as<ECS::Entity>();
		auto  children = node["Children"];
		for (auto child : children)
		{
			auto childEntity = child["Entity"].as<ECS::Entity>();
			hierarchy.mChildren.push_back(childEntity);
			DeserializeEntity(scene, child);
		}
	}

	void SceneSerializer::DeserializeScene(Scene& scene, std::string scenePath)
	{
		LOGINFO("DeserializeScene: ", scenePath, ".yaml");

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(scenePath + ".yaml");
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", scenePath, ".yaml", " with error: ", e.what());
			return;
		}

		scene.mSceneName = data["Scene name"].as<std::string>();

		auto sceneData = data["Scene"];
		if (sceneData)
		{
			for (auto entity : sceneData)
			{
				DeserializeEntity(scene, entity);
			}
		}
	}
}




