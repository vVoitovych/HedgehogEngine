#include "SceneSerializer.hpp"
#include "Scene.hpp"
#include "SceneComponents/TransformComponent.hpp"
#include "SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"

#include "HedgehogMath/Vector.hpp"

#include "Logger/Logger.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace YAML {

	template<>
	struct convert<HM::Vector2>
	{
		static Node encode(const HM::Vector2& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<HM::Vector3>
	{
		static Node encode(const HM::Vector3& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.push_back(rhs.z());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			rhs.z() = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<HM::Vector4>
	{
		static Node encode(const HM::Vector4& rhs)
		{
			Node node;
			node.push_back(rhs.x());
			node.push_back(rhs.y());
			node.push_back(rhs.z());
			node.push_back(rhs.w());
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, HM::Vector4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x() = node[0].as<float>();
			rhs.y() = node[1].as<float>();
			rhs.z() = node[2].as<float>();
			rhs.w() = node[3].as<float>();
			return true;
		}
	};
}

namespace Scene
{

	static YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
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

			out << YAML::Key << "Position" << YAML::Value << transform.mPosition;
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

			out << YAML::EndMap; 
		}
		if (scene.HasLightComponent(entity))
		{ // light component
			auto& light = scene.GetLightComponent(entity);
			out << YAML::Key << "LightComponent";
			out << YAML::BeginMap;

			out << YAML::Key << "LightEnabled" << YAML::Value << light.m_Enable;
			out << YAML::Key << "LightType" << YAML::Value << static_cast<size_t>(light.m_LightType);
			out << YAML::Key << "LightColor" << YAML::Value << light.m_Color;
			out << YAML::Key << "LightIntencity" << YAML::Value << light.m_Intencity;
			out << YAML::Key << "LightRadius" << YAML::Value << light.m_Radius;
			out << YAML::Key << "LightConeAngle" << YAML::Value << light.m_ConeAngle;

			out << YAML::EndMap;
		}
		if (scene.HasRenderComponent(entity))
		{ // render component
			auto& renderComponent = scene.GetRenderComponent(entity);
			out << YAML::Key << "RenderComponent";
			out << YAML::BeginMap;

			out << YAML::Key << "Visible" << YAML::Value << renderComponent.mIsVisible;
			out << YAML::Key << "Material" << YAML::Value << renderComponent.mMaterial;

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

	std::string GetSceneName(std::string inPath)
	{
		return std::filesystem::path(inPath).stem().string();
	}

	void SceneSerializer::SerializeScene(Scene& scene, std::string scenePath)
	{
		LOGINFO("SerializeScene: ", scenePath);
		std::string sceneName = GetSceneName(scenePath);
		scene.m_SceneName = sceneName;
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene name" << YAML::Value << sceneName;
		out << YAML::Key << "Scene" << YAML::Value << YAML::BeginSeq;
		
		SerializeEntity(out, scene, scene.GetRoot());

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(scenePath);
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
			transform.mPosition = transformData["Position"].as<HM::Vector3>();
			transform.mRotation = transformData["Rotation"].as<HM::Vector3>();
			transform.mScale = transformData["Scale"].as<HM::Vector3>();
		}

		auto mesh = node["MeshComponent"];
		if (mesh)
		{
			scene.AddMeshComponent(entity);
			auto& meshComponent = scene.GetMeshComponent(entity);
			meshComponent.mMeshPath = mesh["MeshPath"].as<std::string>();
		}

		auto light = node["LightComponent"];
		if (light)
		{
			scene.AddLightComponent(entity);
			auto& lightComponent = scene.GetLightComponent(entity);
			lightComponent.m_Enable = light["LightEnabled"].as<bool>();
			lightComponent.m_LightType = static_cast<LightType>(light["LightType"].as<size_t>());
			lightComponent.m_Color = light["LightColor"].as<HM::Vector3>();
			lightComponent.m_Intencity = light["LightIntencity"].as<float>();
			lightComponent.m_Radius = light["LightRadius"].as<float>();
			lightComponent.m_ConeAngle = light["LightConeAngle"].as<float>();

		}

		auto renderComponentData = node["RenderComponent"];
		if (renderComponentData)
		{
			scene.AddRenderComponent(entity);
			auto& renderComponent = scene.GetRenderComponent(entity);
			renderComponent.mIsVisible = renderComponentData["Visible"].as<bool>();
			renderComponent.mMaterial = renderComponentData["Material"].as<std::string>();

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
		LOGINFO("DeserializeScene: ", scenePath);

		YAML::Node data;
		try
		{
			data = YAML::LoadFile(scenePath);
		}
		catch (YAML::ParserException e)
		{
			LOGERROR("Failed to load scene: ", scenePath, " with error: ", e.what());
			return;
		}

		scene.m_SceneName = data["Scene name"].as<std::string>();

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




