#include "HedgehogEngine/api/ECS/EcsSerializer.hpp"

#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/HierarchyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

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

namespace HedgehogEngine
{
namespace
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    struct YamlWriter
    {
        YAML::Emitter& m_Out;

        template<typename T>
        void operator()(const char* key, T& val)
        {
            if constexpr (std::is_enum_v<T>)
                m_Out << YAML::Key << key << YAML::Value << static_cast<size_t>(val);
            else
                m_Out << YAML::Key << key << YAML::Value << val;
        }
    };

    struct YamlReader
    {
        const YAML::Node& m_Node;

        template<typename T>
        void operator()(const char* key, T& val)
        {
            if (!m_Node[key]) return;
            if constexpr (std::is_enum_v<T>)
                val = static_cast<T>(m_Node[key].as<size_t>());
            else
                val = m_Node[key].as<T>();
        }
    };

    void SerializeEntity(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity)
    {
        auto& hierarchy = ecs.GetComponent<Scene::HierarchyComponent>(entity);

        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << entity;
        out << YAML::Key << "Name"   << YAML::Value << hierarchy.m_Name;
        out << YAML::Key << "Parent" << YAML::Value << hierarchy.m_Parent;

        {
            auto& transform = ecs.GetComponent<Scene::TransformComponent>(entity);
            out << YAML::Key << "TransformComponent" << YAML::BeginMap;
            YamlWriter w{out};
            const_cast<Scene::TransformComponent&>(transform).Visit(w);
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::MeshComponent>(entity))
        {
            auto& mesh = ecs.GetComponent<Scene::MeshComponent>(entity);
            out << YAML::Key << "MeshComponent" << YAML::BeginMap;
            YamlWriter w{out};
            const_cast<Scene::MeshComponent&>(mesh).Visit(w);
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::LightComponent>(entity))
        {
            auto& light = ecs.GetComponent<Scene::LightComponent>(entity);
            out << YAML::Key << "LightComponent" << YAML::BeginMap;
            YamlWriter w{out};
            const_cast<Scene::LightComponent&>(light).Visit(w);
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::RenderComponent>(entity))
        {
            auto& render = ecs.GetComponent<Scene::RenderComponent>(entity);
            out << YAML::Key << "RenderComponent" << YAML::BeginMap;
            YamlWriter w{out};
            const_cast<Scene::RenderComponent&>(render).Visit(w);
            out << YAML::EndMap;
        }

        if (ecs.HasComponent<Scene::ScriptComponent>(entity))
        {
            auto& script = ecs.GetComponent<Scene::ScriptComponent>(entity);
            out << YAML::Key << "ScriptComponent" << YAML::BeginMap;
            YamlWriter w{out};
            const_cast<Scene::ScriptComponent&>(script).Visit(w);
            if (!script.m_Params.empty())
            {
                out << YAML::Key << "ScriptParams" << YAML::BeginMap;
                for (auto& param : script.m_Params)
                {
                    out << YAML::Key << param.first << YAML::BeginMap;
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
            SerializeEntity(out, ecs, child);
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
            YamlReader r{transformData};
            transform.Visit(r);
        }

        auto meshNode = node["MeshComponent"];
        if (meshNode)
        {
            ecs.AddComponent(entity, Scene::MeshComponent{});
            auto& mesh = ecs.GetComponent<Scene::MeshComponent>(entity);
            YamlReader r{meshNode};
            mesh.Visit(r);
        }

        auto lightNode = node["LightComponent"];
        if (lightNode)
        {
            ecs.AddComponent(entity, Scene::LightComponent{});
            auto& light = ecs.GetComponent<Scene::LightComponent>(entity);
            YamlReader r{lightNode};
            light.Visit(r);
            // Backward-compat: old files used "LightIntencity" (typo)
            if (!lightNode["LightIntensity"] && lightNode["LightIntencity"])
                light.m_Intensity = lightNode["LightIntencity"].as<float>();
        }

        auto renderNode = node["RenderComponent"];
        if (renderNode)
        {
            ecs.AddComponent(entity, Scene::RenderComponent{});
            auto& render = ecs.GetComponent<Scene::RenderComponent>(entity);
            YamlReader r{renderNode};
            render.Visit(r);
        }

        auto scriptNode = node["ScriptComponent"];
        if (scriptNode)
        {
            ecs.AddComponent(entity, Scene::ScriptComponent{});
            auto& script = ecs.GetComponent<Scene::ScriptComponent>(entity);
            YamlReader r{scriptNode};
            script.Visit(r);

            auto parameters = scriptNode["ScriptParams"];
            if (parameters && parameters.IsMap())
            {
                for (const auto& param : parameters)
                {
                    std::string               paramName = param.first.as<std::string>();
                    auto                      data      = param.second;
                    Scene::ParamType          type      = static_cast<Scene::ParamType>(data["ParamType"].as<size_t>());
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
                    script.m_Params[paramName] = { type, value, false };
                }
            }
            scriptSystem.InitScript(entity, ecs);
        }

        hierarchy.m_Name   = node["Name"].as<std::string>();
        hierarchy.m_Parent = node["Parent"].as<ECS::Entity>();

        auto children = node["Children"];
        for (auto child : children)
        {
            hierarchy.m_Children.push_back(child["Entity"].as<ECS::Entity>());
            DeserializeEntity(ecs, child, scriptSystem);
        }
    }
}

    void EcsSerializer::Serialize(const ECS::ECS& ecs, ECS::Entity root,
                                   const std::string& sceneName, const std::string& filePath)
    {
        LOGINFO("EcsSerializer::Serialize: ", filePath);

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

    void EcsSerializer::Deserialize(ECS::ECS& ecs, ECS::Entity& outRoot,
                                     std::string& outSceneName, const std::string& filePath,
                                     Scene::HierarchySystem& hierarchySystem,
                                     Scene::ScriptSystem& scriptSystem)
    {
        LOGINFO("EcsSerializer::Deserialize: ", filePath);

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
