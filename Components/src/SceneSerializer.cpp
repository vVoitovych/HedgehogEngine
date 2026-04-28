#include "Components/api/SceneSerializer.hpp"

#include "Components/api/HierarchySystem.hpp"
#include "Components/api/ScriptSystem.hpp"
#include "Components/api/TransformComponent.hpp"
#include "Components/api/HierarchyComponent.hpp"
#include "Components/api/MeshComponent.hpp"
#include "Components/api/LightComponent.hpp"
#include "Components/api/RenderComponent.hpp"
#include "Components/api/ScriptComponent.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace YAML
{
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

namespace Components
{
namespace
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    std::string GetSceneNameFromPath(const std::string& inPath)
    {
        return std::filesystem::path(inPath).stem().string();
    }

    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy = ecs.GetComponent<Scene::HierarchyComponent>(entity);
        auto& transform = ecs.GetComponent<Scene::TransformComponent>(entity);

        out << YAML::BeginMap;
        out << YAML::Key << "Entity"  << YAML::Value << entity;
        out << YAML::Key << "Name"    << YAML::Value << hierarchy.m_Name;
        out << YAML::Key << "Parent"  << YAML::Value << hierarchy.m_Parent;

        {
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << transform.m_Position;
            out << YAML::Key << "Rotation" << YAML::Value << transform.m_Rotation;
            out << YAML::Key << "Scale"    << YAML::Value << transform.m_Scale;
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::MeshComponent>(entity))
        {
            auto& mesh = ecs.GetComponent<Scene::MeshComponent>(entity);
            out << YAML::Key << "MeshComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "MeshPath" << YAML::Value << mesh.m_MeshPath;
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::LightComponent>(entity))
        {
            auto& light = ecs.GetComponent<Scene::LightComponent>(entity);
            out << YAML::Key << "LightComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "LightEnabled"   << YAML::Value << light.m_Enable;
            out << YAML::Key << "LightType"      << YAML::Value << static_cast<size_t>(light.m_LightType);
            out << YAML::Key << "LightColor"     << YAML::Value << light.m_Color;
            out << YAML::Key << "LightIntensity" << YAML::Value << light.m_Intensity;
            out << YAML::Key << "LightRadius"    << YAML::Value << light.m_Radius;
            out << YAML::Key << "LightConeAngle" << YAML::Value << light.m_ConeAngle;
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::RenderComponent>(entity))
        {
            auto& renderComponent = ecs.GetComponent<Scene::RenderComponent>(entity);
            out << YAML::Key << "RenderComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Visible"  << YAML::Value << renderComponent.m_IsVisible;
            out << YAML::Key << "Material" << YAML::Value << renderComponent.m_Material;
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::ScriptComponent>(entity))
        {
            auto& scriptComponent = ecs.GetComponent<Scene::ScriptComponent>(entity);
            out << YAML::Key << "ScriptComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "ScriptEnable" << YAML::Value << scriptComponent.m_Enable;
            out << YAML::Key << "ScriptFile"   << YAML::Value << scriptComponent.m_ScriptPath;
            if (!scriptComponent.m_Params.empty())
            {
                out << YAML::Key << "ScriptParams";
                out << YAML::BeginMap;
                for (auto& param : scriptComponent.m_Params)
                {
                    out << YAML::Key << param.first;
                    out << YAML::BeginMap;
                    out << YAML::Key << "ParamType" << YAML::Value << static_cast<size_t>(param.second.type);
                    switch (param.second.type)
                    {
                    case Scene::ParamType::Boolean:
                        out << YAML::Key << "ParamValue" << YAML::Value << std::get<bool>(param.second.value);
                        break;
                    case Scene::ParamType::Number:
                        out << YAML::Key << "ParamValue" << YAML::Value << std::get<float>(param.second.value);
                        break;
                    default:
                        break;
                    }
                    out << YAML::EndMap;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }

        out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
        for (auto child : hierarchy.m_Children)
        {
            SerializeEntity(out, ecs, child);
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    void DeserializeEntity(ECS::ECS& ecs, YAML::Node node, Scene::ScriptSystem& scriptSystem)
    {
        ECS::Entity entity = node["Entity"].as<ECS::Entity>();
        ecs.CreateEntity(entity);
        ecs.AddComponent(entity, Scene::TransformComponent{});
        ecs.AddComponent(entity, Scene::HierarchyComponent{});

        auto& transform = ecs.GetComponent<Scene::TransformComponent>(entity);
        auto& hierarchy = ecs.GetComponent<Scene::HierarchyComponent>(entity);

        auto transformData = node["TransformComponent"];
        if (transformData)
        {
            transform.m_Position = transformData["Position"].as<HM::Vector3>();
            transform.m_Rotation = transformData["Rotation"].as<HM::Vector3>();
            transform.m_Scale    = transformData["Scale"].as<HM::Vector3>();
        }

        auto meshNode = node["MeshComponent"];
        if (meshNode)
        {
            ecs.AddComponent(entity, Scene::MeshComponent{});
            auto& meshComponent      = ecs.GetComponent<Scene::MeshComponent>(entity);
            meshComponent.m_MeshPath = meshNode["MeshPath"].as<std::string>();
        }

        auto lightNode = node["LightComponent"];
        if (lightNode)
        {
            ecs.AddComponent(entity, Scene::LightComponent{});
            auto& lightComponent       = ecs.GetComponent<Scene::LightComponent>(entity);
            lightComponent.m_Enable    = lightNode["LightEnabled"].as<bool>();
            lightComponent.m_LightType = static_cast<Scene::LightType>(lightNode["LightType"].as<size_t>());
            lightComponent.m_Color     = lightNode["LightColor"].as<HM::Vector3>();
            if (lightNode["LightIntensity"])
                lightComponent.m_Intensity = lightNode["LightIntensity"].as<float>();
            else if (lightNode["LightIntencity"])
                lightComponent.m_Intensity = lightNode["LightIntencity"].as<float>();
            lightComponent.m_Radius    = lightNode["LightRadius"].as<float>();
            lightComponent.m_ConeAngle = lightNode["LightConeAngle"].as<float>();
        }

        auto renderNode = node["RenderComponent"];
        if (renderNode)
        {
            ecs.AddComponent(entity, Scene::RenderComponent{});
            auto& renderComponent       = ecs.GetComponent<Scene::RenderComponent>(entity);
            renderComponent.m_IsVisible = renderNode["Visible"].as<bool>();
            renderComponent.m_Material  = renderNode["Material"].as<std::string>();
        }

        auto scriptNode = node["ScriptComponent"];
        if (scriptNode)
        {
            ecs.AddComponent(entity, Scene::ScriptComponent{});
            auto& scriptComponent        = ecs.GetComponent<Scene::ScriptComponent>(entity);
            scriptComponent.m_Enable     = scriptNode["ScriptEnable"].as<bool>();
            scriptComponent.m_ScriptPath = scriptNode["ScriptFile"].as<std::string>();

            auto parameters = scriptNode["ScriptParams"];
            if (parameters && parameters.IsMap())
            {
                for (const auto& param : parameters)
                {
                    std::string           paramName = param.first.as<std::string>();
                    auto                  data      = param.second;
                    Scene::ParamType      type      = static_cast<Scene::ParamType>(data["ParamType"].as<size_t>());
                    std::variant<bool, float> value;
                    switch (type)
                    {
                    case Scene::ParamType::Boolean:
                        value = data["ParamValue"].as<bool>();
                        break;
                    case Scene::ParamType::Number:
                        value = data["ParamValue"].as<float>();
                        break;
                    default:
                        break;
                    }
                    scriptComponent.m_Params[paramName] = { type, value, false };
                }
            }
            scriptSystem.InitScript(entity, ecs);
        }

        hierarchy.m_Name   = node["Name"].as<std::string>();
        hierarchy.m_Parent = node["Parent"].as<ECS::Entity>();

        auto children = node["Children"];
        for (auto child : children)
        {
            auto childEntity = child["Entity"].as<ECS::Entity>();
            hierarchy.m_Children.push_back(childEntity);
            DeserializeEntity(ecs, child, scriptSystem);
        }
    }
}

    void SceneSerializer::SerializeScene(const ECS::ECS& ecs, ECS::Entity root,
                                        const std::string& sceneName, const std::string& filePath)
    {
        LOGINFO("SerializeScene: ", filePath);

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene name" << YAML::Value << sceneName;
        out << YAML::Key << "Scene"      << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, ecs, root);
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(filePath);
        fout << out.c_str();
    }

    void SceneSerializer::DeserializeScene(ECS::ECS& ecs, ECS::Entity& outRoot,
                                           std::string& outSceneName, const std::string& filePath,
                                           Scene::HierarchySystem& hierarchySystem,
                                           Scene::ScriptSystem& scriptSystem)
    {
        LOGINFO("DeserializeScene: ", filePath);

        YAML::Node data;
        try
        {
            data = YAML::LoadFile(filePath);
        }
        catch (const YAML::ParserException& e)
        {
            LOGERROR("Failed to load scene: ", filePath, " with error: ", e.what());
            return;
        }

        outSceneName = data["Scene name"].as<std::string>();

        auto sceneData = data["Scene"];
        if (sceneData && sceneData.size() > 0)
        {
            outRoot = sceneData[0]["Entity"].as<ECS::Entity>();
            for (auto node : sceneData)
                DeserializeEntity(ecs, node, scriptSystem);
        }

        hierarchySystem.SetRoot(outRoot);
    }
}
