#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogMath/api/Vector.hpp"
#include "Prefab/IPrefabProvider.hpp"
#include "Reflection/YamlReflection.hpp"

#include "yaml-cpp/yaml.h"

#include <functional>
#include <span>
#include <string>
#include <string_view>
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

namespace EcsSerialization
{
    inline YAML::Emitter& operator<<(YAML::Emitter& out, const HM::Vector3& v)
    {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x() << v.y() << v.z() << YAML::EndSeq;
        return out;
    }

    struct YamlWriter
    {
        YAML::Emitter& Out;

        template<typename T>
        void operator()(const char* key, T& val)
        {
            if constexpr (std::is_enum_v<T>)
                Out << YAML::Key << key << YAML::Value << static_cast<size_t>(val);
            else
                Out << YAML::Key << key << YAML::Value << val;
        }
    };

    struct YamlReader
    {
        const YAML::Node& Node;

        template<typename T>
        void operator()(const char* key, T& val)
        {
            if (!Node[key]) return;
            if constexpr (std::is_enum_v<T>)
                val = static_cast<T>(Node[key].as<size_t>());
            else
                val = Node[key].as<T>();
        }
    };

    // The entity a component's reference should now name, given the one it names.
    using EntityRemap = std::function<ECS::Entity(ECS::Entity)>;

    // Rewrites every ECS::Entity property flagged EntityRef through remap.
    inline void RemapEntityProperties(void* comp, std::span<const Reflection::PropertyDescriptor> props, const EntityRemap& remap)
    {
        for (const auto& prop : props)
        {
            if (prop.type == Reflection::TypeTag::Entity && HasFlag(prop.flags, Reflection::PropertyFlags::EntityRef))
            {
                ECS::Entity& entity = *Reflection::FieldPtr<ECS::Entity>(comp, prop);
                entity              = remap(entity);
            }
        }
    }

    // Collects the keys a Visit names.
    struct YamlKeyFinder
    {
        std::string_view Wanted;
        bool             Found = false;

        template<typename T>
        void operator()(const char* key, T&)
        {
            Found = Found || Wanted == key;
        }
    };

    struct ComponentHandler
    {
        std::string YamlKey;
        std::function<void(YAML::Emitter&, const ECS::ECS&, ECS::Entity)> Serialize;
        // Reads the node into the entity's component, adding it when missing; a key the node
        // lacks keeps the component's value, so a partial node applies a prefab override.
        std::function<void(ECS::ECS&, ECS::Entity, const YAML::Node&)>    Deserialize;
        std::function<bool(const ECS::ECS&, ECS::Entity)>                  HasComponent;
        // Points the component's entity references at their new ids after a subtree is
        // instantiated; empty for a component that holds none.
        std::function<void(ECS::ECS&, ECS::Entity, const EntityRemap&)>   RemapEntities;
        // Whether the component writes a key of that name; empty when unknown (a custom one).
        std::function<bool(std::string_view)>                             HasProperty;
        // Removes the component (reverting a component added on a prefab instance).
        std::function<void(ECS::ECS&, ECS::Entity)>                       Remove;
        // Rewrites the entity ids in the component's YAML map (as Serialize writes it) through
        // remap, as RemapEntities does to the live component; empty for a component that names
        // no entity. Prefab overrides compare and apply values across a prefab's and an instance's
        // ids with it.
        std::function<void(YAML::Node&, const EntityRemap&)>              RemapYaml;
    };

    class ComponentSerializerRegistry
    {
    public:
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
                },
                {},
                [](std::string_view name)
                {
                    T             probe{};
                    YamlKeyFinder finder{ name };
                    probe.Visit(finder);
                    return finder.Found;
                },
                [](ECS::ECS& ecs, ECS::Entity e) { ecs.RemoveComponent<T>(e); }
            });
        }

        template<typename T>
        void RegisterReflected(const char* yamlKey)
        {
            std::string key(yamlKey);
            m_Handlers.push_back({
                key,
                [key](YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity e)
                {
                    auto& comp = ecs.GetComponent<T>(e);
                    out << YAML::Key << key.c_str() << YAML::BeginMap;
                    Reflection::YamlSerializeComponent(out, const_cast<T*>(&comp), T::GetProperties());
                    out << YAML::EndMap;
                },
                [](ECS::ECS& ecs, ECS::Entity e, const YAML::Node& node)
                {
                    if (!ecs.HasComponent<T>(e))
                        ecs.AddComponent(e, T{});
                    T& comp = ecs.GetComponent<T>(e);
                    Reflection::YamlDeserializeComponent(&comp, node, T::GetProperties());
                },
                [](const ECS::ECS& ecs, ECS::Entity e)
                {
                    return ecs.HasComponent<T>(e);
                },
                [](ECS::ECS& ecs, ECS::Entity e, const EntityRemap& remap)
                {
                    RemapEntityProperties(&ecs.GetComponent<T>(e), T::GetProperties(), remap);
                },
                [](std::string_view name)
                {
                    for (const auto& prop : T::GetProperties())
                    {
                        if (name == prop.name)
                            return true;
                    }
                    return false;
                },
                [](ECS::ECS& ecs, ECS::Entity e) { ecs.RemoveComponent<T>(e); },
                [](YAML::Node& component, const EntityRemap& remap)
                {
                    for (const auto& prop : T::GetProperties())
                    {
                        if (prop.type != Reflection::TypeTag::Entity || !HasFlag(prop.flags, Reflection::PropertyFlags::EntityRef))
                            continue;
                        if (YAML::Node value = component[prop.name]; value && value.IsScalar())
                            component[prop.name] = remap(value.as<ECS::Entity>());
                    }
                }
            });
        }

        void RegisterCustom(
            const char*                                                        yamlKey,
            std::function<void(YAML::Emitter&, const ECS::ECS&, ECS::Entity)> serialize,
            std::function<void(ECS::ECS&, ECS::Entity, const YAML::Node&)>    deserialize,
            std::function<bool(const ECS::ECS&, ECS::Entity)>                 hasComponent,
            std::function<void(ECS::ECS&, ECS::Entity, const EntityRemap&)>   remapEntities = {},
            std::function<void(ECS::ECS&, ECS::Entity)>                       remove        = {},
            std::function<void(YAML::Node&, const EntityRemap&)>              remapYaml     = {})
        {
            m_Handlers.push_back({ yamlKey, std::move(serialize), std::move(deserialize), std::move(hasComponent),
                                   std::move(remapEntities), {}, std::move(remove), std::move(remapYaml) });
        }

        const std::vector<ComponentHandler>& GetHandlers() const { return m_Handlers; }

        // Removes the handler writing key; false when there is none.
        bool Unregister(std::string_view key)
        {
            return std::erase_if(m_Handlers, [key](const ComponentHandler& handler) { return handler.YamlKey == key; }) > 0;
        }

        // The handler writing key, or nullptr.
        [[nodiscard]] const ComponentHandler* FindHandler(std::string_view key) const
        {
            for (const ComponentHandler& handler : m_Handlers)
            {
                if (handler.YamlKey == key)
                    return &handler;
            }
            return nullptr;
        }

        // Who knows about prefab instances (the engine's PrefabManager); null writes and reads
        // every entity in full. The provider must outlive its use here.
        void             SetPrefabProvider(IPrefabProvider* provider) { m_PrefabProvider = provider; }
        IPrefabProvider* GetPrefabProvider() const { return m_PrefabProvider; }

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
            if (!ecs.HasComponent<T>(e))
                ecs.AddComponent(e, T{});
            T& comp = ecs.GetComponent<T>(e);
            YamlReader r{node};
            comp.Visit(r);
        }

    private:
        std::vector<ComponentHandler> m_Handlers;
        IPrefabProvider*              m_PrefabProvider = nullptr;
    };
}
