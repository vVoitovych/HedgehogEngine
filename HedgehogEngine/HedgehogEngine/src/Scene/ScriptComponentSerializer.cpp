#include "HedgehogEngine/api/Scene/ScriptComponentSerializer.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "Logger/api/Logger.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace HedgehogEngine
{
    namespace
    {
        constexpr std::array<std::pair<ScriptPropertyType, std::string_view>, 7> TYPE_NAMES = { {
            { ScriptPropertyType::Number,    "Number" },
            { ScriptPropertyType::Bool,      "Bool" },
            { ScriptPropertyType::Vector3,   "Vector3" },
            { ScriptPropertyType::Color,     "Color" },
            { ScriptPropertyType::String,    "String" },
            { ScriptPropertyType::EntityRef, "EntityRef" },
            { ScriptPropertyType::AssetRef,  "AssetRef" },
        } };

        // The legacy ScriptParams' ParamType values.
        constexpr size_t LEGACY_BOOLEAN = 0;
        constexpr size_t LEGACY_NUMBER  = 1;

        std::string_view TypeName(ScriptPropertyType type)
        {
            for (const auto& [value, name] : TYPE_NAMES)
                if (value == type)
                    return name;
            return "Number";
        }

        std::optional<ScriptPropertyType> TypeFromName(const std::string& name)
        {
            for (const auto& [value, typeName] : TYPE_NAMES)
                if (typeName == name)
                    return value;
            return std::nullopt;
        }

        void WriteValue(YAML::Emitter& out, const ScriptProperty& property)
        {
            out << YAML::Key << "Value" << YAML::Value;
            switch (property.Type)
            {
            case ScriptPropertyType::Number:
                out << std::get<float>(property.Value);
                break;
            case ScriptPropertyType::Bool:
                out << std::get<bool>(property.Value);
                break;
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
                EcsSerialization::operator<<(out, std::get<HM::Vector3>(property.Value));
                break;
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                out << std::get<std::string>(property.Value);
                break;
            case ScriptPropertyType::EntityRef:
                out << std::get<ECS::Entity>(property.Value);
                break;
            }
        }

        // A value node read as the type's alternative; throws YAML::Exception when it is not one.
        ScriptPropertyValue ReadValue(ScriptPropertyType type, const YAML::Node& value)
        {
            switch (type)
            {
            case ScriptPropertyType::Bool:
                return value.as<bool>();
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
                return value.as<HM::Vector3>();
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                return value.as<std::string>();
            case ScriptPropertyType::EntityRef:
                return value.as<ECS::Entity>();
            case ScriptPropertyType::Number:
            default:
                return value.as<float>();
            }
        }

        void Serialize(YAML::Emitter& out, const ECS::ECS& ecs, ECS::Entity entity)
        {
            ScriptComponent& script = ecs.GetComponent<ScriptComponent>(entity);
            out << YAML::Key << "ScriptComponent" << YAML::BeginMap;
            EcsSerialization::YamlWriter writer{ out };
            script.Visit(writer);
            if (!script.Properties.empty())
            {
                out << YAML::Key << "ScriptProperties" << YAML::BeginMap;
                for (const ScriptProperty& property : script.Properties)
                {
                    out << YAML::Key << property.Name << YAML::BeginMap;
                    out << YAML::Key << "Type" << YAML::Value << std::string(TypeName(property.Type));
                    WriteValue(out, property);
                    if (property.Type == ScriptPropertyType::AssetRef)
                        out << YAML::Key << "AssetType" << YAML::Value << property.AssetType;
                    out << YAML::EndMap;
                }
                out << YAML::EndMap;
            }
            out << YAML::EndMap;
        }

        void ReadProperties(ScriptComponent& script, const YAML::Node& properties)
        {
            for (const auto& entry : properties)
            {
                const std::string name = entry.first.as<std::string>();
                try
                {
                    const YAML::Node data = entry.second;
                    const auto       type = TypeFromName(data["Type"].as<std::string>());
                    if (!type)
                    {
                        LOGWARNING(std::string("ScriptComponent: property '") + name
                                   + "' of '" + script.ScriptPath + "' has an unknown Type; skipped.");
                        continue;
                    }
                    ScriptProperty property{ name, *type, ReadValue(*type, data["Value"]), {} };
                    if (*type == ScriptPropertyType::AssetRef && data["AssetType"])
                        property.AssetType = data["AssetType"].as<std::string>();
                    SetScriptProperty(script, std::move(property));
                }
                catch (const YAML::Exception&)
                {
                    LOGWARNING(std::string("ScriptComponent: property '") + name
                               + "' of '" + script.ScriptPath + "' could not be read; skipped.");
                }
            }
        }

        // Scenes saved before ScriptProperties: Number and Boolean ScriptParams.
        void ReadLegacyParams(ScriptComponent& script, const YAML::Node& params)
        {
            for (const auto& entry : params)
            {
                const std::string name = entry.first.as<std::string>();
                try
                {
                    const YAML::Node data      = entry.second;
                    const size_t     paramType = data["ParamType"].as<size_t>();
                    if (paramType == LEGACY_BOOLEAN)
                        SetScriptProperty(script, { name, ScriptPropertyType::Bool, data["ParamValue"].as<bool>(), {} });
                    else if (paramType == LEGACY_NUMBER)
                        SetScriptProperty(script, { name, ScriptPropertyType::Number, data["ParamValue"].as<float>(), {} });
                    else
                        LOGWARNING(std::string("ScriptComponent: parameter '") + name
                                   + "' of '" + script.ScriptPath + "' has an unknown ParamType; skipped.");
                }
                catch (const YAML::Exception&)
                {
                    LOGWARNING(std::string("ScriptComponent: parameter '") + name
                               + "' of '" + script.ScriptPath + "' could not be read; skipped.");
                }
            }
        }

        void Deserialize(ECS::ECS& ecs, ECS::Entity entity, const YAML::Node& node)
        {
            EcsSerialization::ComponentSerializerRegistry::DeserializeWithVisit<ScriptComponent>(ecs, entity, node);
            ScriptComponent& script = ecs.GetComponent<ScriptComponent>(entity);

            const YAML::Node properties = node["ScriptProperties"];
            const YAML::Node params     = node["ScriptParams"];
            if (properties && properties.IsMap())
            {
                if (params)
                    LOGWARNING(std::string("ScriptComponent: '") + script.ScriptPath
                               + "' has both ScriptProperties and ScriptParams; ScriptParams is ignored.");
                ReadProperties(script, properties);
            }
            else if (params && params.IsMap())
            {
                ReadLegacyParams(script, params);
            }
        }
    }

    void RegisterScriptComponentSerializer(EcsSerialization::ComponentSerializerRegistry& registry)
    {
        registry.RegisterCustom(
            "ScriptComponent", Serialize, Deserialize,
            [](const ECS::ECS& ecs, ECS::Entity entity) { return ecs.HasComponent<ScriptComponent>(entity); },
            [](ECS::ECS& ecs, ECS::Entity entity, const EcsSerialization::EntityRemap& remap)
            {
                for (ScriptProperty& property : ecs.GetComponent<ScriptComponent>(entity).Properties)
                {
                    if (ECS::Entity* target = std::get_if<ECS::Entity>(&property.Value))
                        *target = remap(*target);
                }
            },
            [](ECS::ECS& ecs, ECS::Entity entity) { ecs.RemoveComponent<ScriptComponent>(entity); },
            [](YAML::Node& component, const EcsSerialization::EntityRemap& remap)
            {
                YAML::Node properties = component["ScriptProperties"];
                if (!properties || !properties.IsMap())
                    return;
                for (auto entry : properties)
                {
                    YAML::Node property = entry.second;
                    if (property.IsMap() && property["Type"] && property["Type"].as<std::string>() == TypeName(ScriptPropertyType::EntityRef) &&
                        property["Value"] && property["Value"].IsScalar())
                        property["Value"] = remap(property["Value"].as<ECS::Entity>());
                }
            });
    }
}
