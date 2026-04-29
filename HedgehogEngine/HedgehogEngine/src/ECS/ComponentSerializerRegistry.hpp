#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "yaml-cpp/yaml.h"

#include <functional>
#include <string>
#include <vector>

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
            if (!node.IsSequence() || node.size() != 2) return false;
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
            if (!node.IsSequence() || node.size() != 3) return false;
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
            if (!node.IsSequence() || node.size() != 4) return false;
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
    // Emitter overload for Vector3 used by YamlWriter
    inline YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    // Visitor that writes component fields to a YAML emitter
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

    // Visitor that reads component fields from a YAML node (skips missing keys)
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

    struct ComponentHandler
    {
        std::string m_YamlKey;
        std::function<void(YAML::Emitter&, const ECS::ECS&, ECS::Entity)> m_Serialize;
        std::function<void(ECS::ECS&, ECS::Entity, const YAML::Node&)>    m_Deserialize;
        std::function<bool(const ECS::ECS&, ECS::Entity)>                  m_HasComponent;
    };

    class ComponentSerializerRegistry
    {
    public:
        // Register a component that exposes all serializable fields through Visit()
        template<typename T>
        void RegisterVisitable(const char* yamlKey)
        {
            std::string key(yamlKey);
            m_Handlers.push_back({
                key,
                [key](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
                {
                    SerializeWithVisit<T>(out, ecs, e, key.c_str());
                },
                [](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
                {
                    DeserializeWithVisit<T>(ecs, e, node);
                },
                [](const ECS::ECS& ecs, ECS::Entity e)
                {
                    return ecs.HasComponent<T>(e);
                }
            });
        }

        // Register a component with hand-written serialize/deserialize logic
        void RegisterCustom(
            const char*                                                        yamlKey,
            std::function<void(YAML::Emitter&, const ECS::ECS&, ECS::Entity)> serialize,
            std::function<void(ECS::ECS&, ECS::Entity, const YAML::Node&)>    deserialize,
            std::function<bool(const ECS::ECS&, ECS::Entity)>                 hasComponent)
        {
            m_Handlers.push_back({ yamlKey, std::move(serialize), std::move(deserialize), std::move(hasComponent) });
        }

        const std::vector<ComponentHandler>& GetHandlers() const { return m_Handlers; }

        // Helpers available to custom lambda bodies
        template<typename T>
        static void SerializeWithVisit(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e, const char* key)
        {
            T& comp = ecs.GetComponent<T>(e);
            out << YAML::Key << key << YAML::BeginMap;
            YamlWriter w{out};
            comp.Visit(w);
            out << YAML::EndMap;
        }

        template<typename T>
        static void DeserializeWithVisit(ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
        {
            ecs.AddComponent(e, T{});
            T& comp = ecs.GetComponent<T>(e);
            YamlReader r{node};
            comp.Visit(r);
        }

    private:
        std::vector<ComponentHandler> m_Handlers;
    };
}
