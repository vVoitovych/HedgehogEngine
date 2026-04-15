#include "SceneSerializer.hpp"
#include "Scene.hpp"
#include "SceneComponents/TransformComponent.hpp"
#include "SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include "Logger/api/Logger.hpp"

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
namespace
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    std::string GetSceneName(const std::string& inPath)
    {
        return std::filesystem::path(inPath).stem().string();
    }

    void SerializeEntity(YAML::Emitter& out, Scene& scene, ECS::Entity entity)
    {
        auto& hierarchy = scene.GetHierarchyComponent(entity);
        auto& transform = scene.GetTransformComponent(entity);

        out << YAML::BeginMap;
        out << YAML::Key << "Entity"  << YAML::Value << entity;
        out << YAML::Key << "Name"    << YAML::Value << hierarchy.m_Name;
        out << YAML::Key << "Parent"  << YAML::Value << hierarchy.m_Parent;

        { // TransformComponent
            out << YAML::Key << "TransformComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Position" << YAML::Value << transform.m_Position;
            out << YAML::Key << "Rotation" << YAML::Value << transform.m_Rotation;
            out << YAML::Key << "Scale"    << YAML::Value << transform.m_Scale;
            out << YAML::EndMap;
        }

        if (scene.HasMeshComponent(entity))
        {
            auto& mesh = scene.GetMeshComponent(entity);
            out << YAML::Key << "MeshComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "MeshPath" << YAML::Value << mesh.m_MeshPath;
            out << YAML::EndMap;
        }

        if (scene.HasLightComponent(entity))
        {
            auto& light = scene.GetLightComponent(entity);
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

        if (scene.HasRenderComponent(entity))
        {
            auto& renderComponent = scene.GetRenderComponent(entity);
            out << YAML::Key << "RenderComponent";
            out << YAML::BeginMap;
            out << YAML::Key << "Visible"  << YAML::Value << renderComponent.m_IsVisible;
            out << YAML::Key << "Material" << YAML::Value << renderComponent.m_Material;
            out << YAML::EndMap;
        }

        if (scene.HasScriptComponent(entity))
        {
            auto& scriptComponent = scene.GetScriptComponent(entity);
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
                    case ParamType::Boolean:
                        out << YAML::Key << "ParamValue" << YAML::Value << std::get<bool>(param.second.value);
                        break;
                    case ParamType::Number:
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
            SerializeEntity(out, scene, child);
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }

    void DeserializeEntity(Scene& scene, YAML::Node node)
    {
        ECS::Entity entity = node["Entity"].as<ECS::Entity>();
        scene.CreateGameObject(entity);

        auto& transform  = scene.GetTransformComponent(entity);
        auto& hierarchy  = scene.GetHierarchyComponent(entity);

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
            scene.AddMeshComponent(entity);
            auto& meshComponent       = scene.GetMeshComponent(entity);
            meshComponent.m_MeshPath  = meshNode["MeshPath"].as<std::string>();
        }

        auto lightNode = node["LightComponent"];
        if (lightNode)
        {
            scene.AddLightComponent(entity);
            auto& lightComponent      = scene.GetLightComponent(entity);
            lightComponent.m_Enable   = lightNode["LightEnabled"].as<bool>();
            lightComponent.m_LightType = static_cast<LightType>(lightNode["LightType"].as<size_t>());
            lightComponent.m_Color    = lightNode["LightColor"].as<HM::Vector3>();
            // Support both the old key ("LightIntencity") and the new key ("LightIntensity")
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
            scene.AddRenderComponent(entity);
            auto& renderComponent         = scene.GetRenderComponent(entity);
            renderComponent.m_IsVisible   = renderNode["Visible"].as<bool>();
            renderComponent.m_Material    = renderNode["Material"].as<std::string>();
        }

        auto scriptNode = node["ScriptComponent"];
        if (scriptNode)
        {
            scene.AddScriptComponent(entity);
            auto& scriptComponent         = scene.GetScriptComponent(entity);
            scriptComponent.m_Enable      = scriptNode["ScriptEnable"].as<bool>();
            scriptComponent.m_ScriptPath  = scriptNode["ScriptFile"].as<std::string>();

            auto parameters = scriptNode["ScriptParams"];
            if (parameters && parameters.IsMap())
            {
                for (const auto& param : parameters)
                {
                    std::string paramName = param.first.as<std::string>();
                    auto        data      = param.second;
                    ParamType   type      = static_cast<ParamType>(data["ParamType"].as<size_t>());
                    std::variant<bool, float> value;
                    switch (type)
                    {
                    case ParamType::Boolean:
                        value = data["ParamValue"].as<bool>();
                        break;
                    case ParamType::Number:
                        value = data["ParamValue"].as<float>();
                        break;
                    default:
                        break;
                    }
                    scriptComponent.m_Params[paramName] = { type, value, false };
                }
            }
            scene.InitScriptComponent(entity);
        }

        hierarchy.m_Name   = node["Name"].as<std::string>();
        hierarchy.m_Parent = node["Parent"].as<ECS::Entity>();

        auto children = node["Children"];
        for (auto child : children)
        {
            auto childEntity = child["Entity"].as<ECS::Entity>();
            hierarchy.m_Children.push_back(childEntity);
            DeserializeEntity(scene, child);
        }
    }
}

    void SceneSerializer::SerializeScene(Scene& scene, std::string scenePath)
    {
        LOGINFO("SerializeScene: ", scenePath);
        const std::string sceneName = GetSceneName(scenePath);
        scene.m_SceneName = sceneName;

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene name" << YAML::Value << sceneName;
        out << YAML::Key << "Scene"      << YAML::Value << YAML::BeginSeq;
        SerializeEntity(out, scene, scene.GetRoot());
        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(scenePath);
        fout << out.c_str();
    }

    void SceneSerializer::DeserializeScene(Scene& scene, std::string scenePath)
    {
        LOGINFO("DeserializeScene: ", scenePath);

        YAML::Node data;
        try
        {
            data = YAML::LoadFile(scenePath);
        }
        catch (const YAML::ParserException& e)
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
