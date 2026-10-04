#include "EcsSerialization/api/Prefab/OverrideSet.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "Logger/api/Logger.hpp"

#include <cstdlib>

namespace EcsSerialization
{
    namespace
    {
        void IndexNode(const YAML::Node& node, std::vector<YAML::Node>& out)
        {
            const size_t local = node["Entity"].as<size_t>();
            if (local >= out.size())
                out.resize(local + 1);
            out[local] = node;
            for (const YAML::Node& child : node["Children"])
                IndexNode(child, out);
        }

        // The number a scalar spells, as the float a component would hold; false when it is not one.
        bool ReadFloat(const std::string& text, float& value)
        {
            if (text.empty())
                return false;
            char*        end    = nullptr;
            const double parsed = std::strtod(text.c_str(), &end);
            if (end != text.c_str() + text.size())
                return false;
            value = static_cast<float>(parsed);
            return true;
        }

        // The entity's components as the scene writes them: a map from handler key to its YAML.
        YAML::Node SerializeComponents(const ComponentSerializerRegistry& registry, const ECS::ECS& ecs, ECS::Entity entity)
        {
            YAML::Emitter out;
            out << YAML::BeginMap;
            for (const auto& handler : registry.GetHandlers())
            {
                if (handler.HasComponent(ecs, entity))
                    handler.Serialize(out, ecs, entity);
            }
            out << YAML::EndMap;
            return YAML::Load(out.c_str());
        }
    }

    std::vector<YAML::Node> IndexSubtree(const YAML::Node& subtreeDocument)
    {
        std::vector<YAML::Node> nodes;
        const YAML::Node        subtree = subtreeDocument["Subtree"];
        if (subtree && subtree.IsSequence() && subtree.size() == 1)
            IndexNode(subtree[0], nodes);
        return nodes;
    }

    bool ValuesEqual(const YAML::Node& a, const YAML::Node& b)
    {
        if (a.Type() != b.Type())
            return false;
        switch (a.Type())
        {
        case YAML::NodeType::Scalar:
        {
            if (a.Scalar() == b.Scalar())
                return true;
            float x = 0.0f;
            float y = 0.0f;
            return ReadFloat(a.Scalar(), x) && ReadFloat(b.Scalar(), y) && x == y;
        }
        case YAML::NodeType::Sequence:
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (!ValuesEqual(a[i], b[i]))
                    return false;
            }
            return true;
        case YAML::NodeType::Map:
            if (a.size() != b.size())
                return false;
            for (const auto& entry : a)
            {
                const YAML::Node other = b[entry.first.Scalar()];
                if (!other || !ValuesEqual(entry.second, other))
                    return false;
            }
            return true;
        case YAML::NodeType::Null:
        case YAML::NodeType::Undefined:
            return true;
        }
        return false;
    }

    OverrideSet DiffInstance(const ComponentSerializerRegistry& registry, const ECS::ECS& ecs,
                             const std::vector<ECS::Entity>& entities, const YAML::Node& subtreeDocument,
                             const std::string& skipComponent)
    {
        OverrideSet                   overrides;
        const std::vector<YAML::Node> nodes = IndexSubtree(subtreeDocument);
        for (size_t local = 0; local < entities.size() && local < nodes.size(); ++local)
        {
            if (entities[local] == ECS::INVALID_ENTITY || !nodes[local])
                continue;
            const YAML::Node instance = SerializeComponents(registry, ecs, entities[local]);
            const YAML::Node prefab   = nodes[local];
            for (const auto& handler : registry.GetHandlers())
            {
                if (handler.YamlKey == skipComponent)
                    continue;
                const YAML::Node mine   = instance[handler.YamlKey];
                const YAML::Node theirs = prefab[handler.YamlKey];
                if (mine && !theirs)
                {
                    overrides.push_back({ static_cast<uint32_t>(local), handler.YamlKey, {}, mine });
                }
                else if (!mine && theirs)
                {
                    LOGWARNING("[Prefab] Node " + std::to_string(local) + " of an instance has no " + handler.YamlKey +
                               ", which its prefab has; removing a component from an instance is not saved.");
                }
                else if (mine && mine.IsMap())
                {
                    for (const auto& property : mine)
                    {
                        const std::string name  = property.first.Scalar();
                        const YAML::Node  value = theirs[name];
                        if (!value || !ValuesEqual(property.second, value))
                            overrides.push_back({ static_cast<uint32_t>(local), handler.YamlKey, name, property.second });
                    }
                }
            }
        }
        return overrides;
    }

    void ApplyOverrides(const ComponentSerializerRegistry& registry, ECS::ECS& ecs, const std::vector<ECS::Entity>& entities,
                        const OverrideSet& overrides, const std::string& sourceName)
    {
        for (const PropertyOverride& entry : overrides)
        {
            const std::string what = sourceName + ": the override of " + entry.Component +
                                     (entry.Property.empty() ? std::string() : "." + entry.Property) + " on node " +
                                     std::to_string(entry.LocalId);
            if (entry.LocalId >= entities.size() || entities[entry.LocalId] == ECS::INVALID_ENTITY)
            {
                LOGWARNING("[Prefab] " + what + " is dropped: the prefab has no such node.");
                continue;
            }
            const ComponentHandler* handler = registry.FindHandler(entry.Component);
            if (!handler)
            {
                LOGWARNING("[Prefab] " + what + " is dropped: no component is called that.");
                continue;
            }
            const ECS::Entity entity = entities[entry.LocalId];
            try
            {
                if (entry.Property.empty())
                {
                    handler->Deserialize(ecs, entity, entry.Value);
                    continue;
                }
                if (!handler->HasComponent(ecs, entity))
                {
                    LOGWARNING("[Prefab] " + what + " is dropped: the prefab's node no longer has the component.");
                    continue;
                }
                if (handler->HasProperty && !handler->HasProperty(entry.Property))
                {
                    LOGWARNING("[Prefab] " + what + " is dropped: the component has no such property.");
                    continue;
                }
                YAML::Node partial;
                partial[entry.Property] = entry.Value;
                handler->Deserialize(ecs, entity, partial);
            }
            catch (const YAML::Exception& e)
            {
                LOGWARNING("[Prefab] " + what + " is dropped: " + e.what());
            }
        }
    }

    void WriteOverrides(YAML::Emitter& out, const OverrideSet& overrides)
    {
        out << YAML::BeginSeq;
        for (const PropertyOverride& entry : overrides)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "LocalId" << YAML::Value << entry.LocalId;
            out << YAML::Key << "Component" << YAML::Value << entry.Component;
            if (!entry.Property.empty())
                out << YAML::Key << "Property" << YAML::Value << entry.Property;
            out << YAML::Key << "Value" << YAML::Value << entry.Value;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
    }

    OverrideSet ReadOverrides(const YAML::Node& node, const std::string& sourceName)
    {
        OverrideSet overrides;
        if (!node || !node.IsSequence())
            return overrides;
        for (const YAML::Node& entry : node)
        {
            try
            {
                PropertyOverride read;
                read.LocalId   = entry["LocalId"].as<uint32_t>();
                read.Component = entry["Component"].as<std::string>();
                if (entry["Property"])
                    read.Property = entry["Property"].as<std::string>();
                read.Value = entry["Value"];
                if (!read.Value)
                    throw YAML::Exception(YAML::Mark::null_mark(), "no Value");
                overrides.push_back(std::move(read));
            }
            catch (const YAML::Exception& e)
            {
                LOGWARNING("[Prefab] " + sourceName + ": an override does not read and is skipped (" + e.what() + ").");
            }
        }
        return overrides;
    }
}
