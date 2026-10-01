#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Bindings/ScriptHandles.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "ECS/api/ECS.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Declared properties. A script lists the values the inspector shows in a top-level table:
//
//   Properties = {
//       speed     = 1.0,                                     -- Number
//       clockWise = true,                                    -- Bool
//       label     = "x",                                     -- String
//       offset    = Vector3(0, 1, 0),                        -- Vector3
//       tint      = Color(1, 0, 0),                          -- Color
//       target    = EntityRef(),                             -- EntityRef
//       mesh      = AssetRef("Mesh"),                        -- AssetRef
//       range     = { type = "number", default = 1, min = 0, max = 10, tooltip = "..." },
//   }
//
// Color, EntityRef and AssetRef are helpers that return the long form. A Lua table has no order,
// so the declarations are listed by name.
namespace HedgehogScripting
{
    namespace
    {
        using HedgehogEngine::ScriptProperty;
        using HedgehogEngine::ScriptPropertyType;
        using HedgehogEngine::ScriptPropertyValue;

        // The long form's type names, and the names warnings use.
        constexpr std::array<std::pair<std::string_view, ScriptPropertyType>, 8> TYPE_NAMES = { {
            { "number", ScriptPropertyType::Number },
            { "bool", ScriptPropertyType::Bool },
            { "boolean", ScriptPropertyType::Bool },
            { "vector3", ScriptPropertyType::Vector3 },
            { "color", ScriptPropertyType::Color },
            { "string", ScriptPropertyType::String },
            { "entity", ScriptPropertyType::EntityRef },
            { "asset", ScriptPropertyType::AssetRef },
        } };

        std::optional<ScriptPropertyType> TypeFromName(const std::string& name)
        {
            for (const auto& [typeName, type] : TYPE_NAMES)
                if (typeName == name)
                    return type;
            return std::nullopt;
        }

        std::string TypeName(ScriptPropertyType type)
        {
            for (const auto& [typeName, value] : TYPE_NAMES)
                if (value == type)
                    return std::string(typeName);
            return "unknown";
        }

        // The type a short-form value declares, if it is one a property can hold.
        std::optional<ScriptPropertyType> TypeOfValue(const sol::object& value)
        {
            switch (value.get_type())
            {
            case sol::type::number:
                return ScriptPropertyType::Number;
            case sol::type::boolean:
                return ScriptPropertyType::Bool;
            case sol::type::string:
                return ScriptPropertyType::String;
            default:
                if (value.is<HM::Vector3>())
                    return ScriptPropertyType::Vector3;
                return std::nullopt;
            }
        }

        ScriptPropertyValue DefaultFor(ScriptPropertyType type)
        {
            switch (type)
            {
            case ScriptPropertyType::Bool:
                return false;
            case ScriptPropertyType::Vector3:
                return HM::Vector3(0.0f, 0.0f, 0.0f);
            case ScriptPropertyType::Color:
                return HM::Vector3(1.0f, 1.0f, 1.0f);
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                return std::string();
            case ScriptPropertyType::EntityRef:
                return ECS::INVALID_ENTITY;
            case ScriptPropertyType::Number:
            default:
                return 0.0f;
            }
        }

        // Whether value holds the variant alternative type uses.
        bool Holds(ScriptPropertyType type, const ScriptPropertyValue& value)
        {
            switch (type)
            {
            case ScriptPropertyType::Number:
                return std::holds_alternative<float>(value);
            case ScriptPropertyType::Bool:
                return std::holds_alternative<bool>(value);
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
                return std::holds_alternative<HM::Vector3>(value);
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                return std::holds_alternative<std::string>(value);
            case ScriptPropertyType::EntityRef:
                return std::holds_alternative<ECS::Entity>(value);
            }
            return false;
        }

        // A declared default read as type; nullopt when it is not one. An EntityRef has no default.
        std::optional<ScriptPropertyValue> ReadValue(ScriptPropertyType type, const sol::object& value)
        {
            switch (type)
            {
            case ScriptPropertyType::Number:
                if (value.get_type() == sol::type::number)
                    return value.as<float>();
                break;
            case ScriptPropertyType::Bool:
                if (value.get_type() == sol::type::boolean)
                    return value.as<bool>();
                break;
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
                if (value.is<HM::Vector3>())
                    return value.as<HM::Vector3>();
                break;
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                if (value.get_type() == sol::type::string)
                    return value.as<std::string>();
                break;
            case ScriptPropertyType::EntityRef:
                break;
            }
            return std::nullopt;
        }

        bool IsNil(const sol::object& value)
        {
            return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
        }

        // One entry of `Properties`; on failure, problem says why.
        std::optional<ScriptPropertyDeclaration> ReadDeclaration(const std::string& name, const sol::object& value,
                                                                 std::string& problem)
        {
            ScriptPropertyDeclaration declaration;
            declaration.Default.Name = name;

            // The short form: the default value itself.
            if (value.get_type() != sol::type::table)
            {
                const auto type = TypeOfValue(value);
                if (!type)
                {
                    problem = "a " + std::string(sol::type_name(value.lua_state(), value.get_type())) +
                              " is not a property value";
                    return std::nullopt;
                }
                declaration.Default.Type  = *type;
                declaration.Default.Value = *ReadValue(*type, value);
                return declaration;
            }

            // The long form: { type = "...", default = ..., min, max, tooltip, assetType }.
            const sol::table  entry       = value.as<sol::table>();
            const sol::object typeObject  = entry["type"];
            const sol::object defaultObject = entry["default"];

            std::optional<ScriptPropertyType> type;
            if (typeObject.get_type() == sol::type::string)
            {
                type = TypeFromName(typeObject.as<std::string>());
                if (!type)
                {
                    problem = "unknown type '" + typeObject.as<std::string>() + "'";
                    return std::nullopt;
                }
            }
            else if (!IsNil(defaultObject))
            {
                type = TypeOfValue(defaultObject);
            }
            if (!type)
            {
                problem = "a declaration needs a type or a default";
                return std::nullopt;
            }

            declaration.Default.Type  = *type;
            declaration.Default.Value = DefaultFor(*type);
            if (!IsNil(defaultObject))
            {
                const auto read = ReadValue(*type, defaultObject);
                if (!read)
                {
                    problem = "its default is not a " + TypeName(*type);
                    return std::nullopt;
                }
                declaration.Default.Value = *read;
            }

            if (*type == ScriptPropertyType::AssetRef)
                declaration.Default.AssetType = entry.get_or<std::string>("assetType", "");
            if (*type == ScriptPropertyType::Number)
            {
                const sol::object min = entry["min"];
                const sol::object max = entry["max"];
                if (min.get_type() == sol::type::number)
                    declaration.Min = min.as<float>();
                if (max.get_type() == sol::type::number)
                    declaration.Max = max.as<float>();
            }
            declaration.Tooltip = entry.get_or<std::string>("tooltip", "");
            return declaration;
        }
    }

    void ScriptSystem::RegisterPropertyHelpers()
    {
        // Each returns the long form, so the declaration reader has one shape to handle.
        m_Lua.set_function("Color", [](sol::this_state state, sol::optional<float> r, sol::optional<float> g,
                                       sol::optional<float> b)
        {
            sol::state_view lua(state);
            sol::table      declaration = lua.create_table();
            declaration["type"]         = "color";
            declaration["default"]      = HM::Vector3(r.value_or(1.0f), g.value_or(1.0f), b.value_or(1.0f));
            return declaration;
        });
        m_Lua.set_function("EntityRef", [](sol::this_state state)
        {
            sol::state_view lua(state);
            sol::table      declaration = lua.create_table();
            declaration["type"]         = "entity";
            return declaration;
        });
        m_Lua.set_function("AssetRef", [](sol::this_state state, const std::string& assetType, sol::optional<std::string> path)
        {
            sol::state_view lua(state);
            sol::table      declaration = lua.create_table();
            declaration["type"]         = "asset";
            declaration["assetType"]    = assetType;
            declaration["default"]      = path.value_or("");
            return declaration;
        });
    }

    std::vector<ScriptPropertyDeclaration> ScriptSystem::ReadDeclarations(const sol::table&  defaults,
                                                                          const std::string& scriptPath) const
    {
        std::vector<ScriptPropertyDeclaration> declarations;
        const sol::object declared = defaults.raw_get<sol::object>("Properties");
        if (IsNil(declared))
            return declarations;
        if (declared.get_type() != sol::type::table)
        {
            LOGWARNING("[Script] " + scriptPath + ": Properties is not a table, so the script declares no properties.");
            return declarations;
        }

        for (const auto& [key, value] : declared.as<sol::table>())
        {
            if (key.get_type() != sol::type::string)
            {
                LOGWARNING("[Script] " + scriptPath + ": a Properties entry without a name is skipped.");
                continue;
            }
            const std::string name = key.as<std::string>();
            std::string       problem;
            if (auto declaration = ReadDeclaration(name, value, problem))
                declarations.push_back(std::move(*declaration));
            else
                LOGWARNING("[Script] " + scriptPath + ": property '" + name + "' is skipped: " + problem + ".");
        }
        std::sort(declarations.begin(), declarations.end(),
                  [](const auto& a, const auto& b) { return a.Default.Name < b.Default.Name; });
        return declarations;
    }

    void ScriptSystem::ApplyProperties(ECS::ECS& ecs, ECS::Entity entity, EntityScript& script, bool warn)
    {
        const auto scriptClass = m_Classes.find(script.ScriptPath);
        if (scriptClass == m_Classes.end())
            return;
        const auto& component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        const std::string where = "[Script] " + script.EntityName + " (" + script.ScriptPath + "): property '";

        for (const ScriptPropertyDeclaration& declaration : scriptClass->second.Declarations)
        {
            // The saved value where it fits the declaration, else the declared default.
            ScriptProperty property = declaration.Default;
            if (const ScriptProperty* saved = HedgehogEngine::FindScriptProperty(component, property.Name))
            {
                if (saved->Type == property.Type && Holds(saved->Type, saved->Value))
                    property.Value = saved->Value;
                else if (warn)
                    LOGWARNING(where + property.Name + "' is saved as " + TypeName(saved->Type) + " but declared as " +
                               TypeName(property.Type) + "; the declared default is used.");
            }

            switch (property.Type)
            {
            case ScriptPropertyType::Number:
                script.Self[property.Name] = std::get<float>(property.Value);
                break;
            case ScriptPropertyType::Bool:
                script.Self[property.Name] = std::get<bool>(property.Value);
                break;
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
                script.Self[property.Name] = std::get<HM::Vector3>(property.Value);
                break;
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:
                script.Self[property.Name] = std::get<std::string>(property.Value);
                break;
            case ScriptPropertyType::EntityRef:
            {
                // A reference to no entity, or to one that is gone, is an invalid handle, not nil.
                const ECS::Entity target = std::get<ECS::Entity>(property.Value);
                script.Self[property.Name] = target < ECS::MAX_ENTITIES && ecs.IsAlive(target)
                                                 ? Bindings::MakeScriptEntity(ecs, target)
                                                 : Bindings::ScriptEntity{};
                break;
            }
            }
        }

        if (!warn)
            return;
        // Saved values the script no longer declares stay in the component until the next save.
        const auto& declarations = scriptClass->second.Declarations;
        for (const ScriptProperty& saved : component.Properties)
        {
            const bool declared = std::any_of(declarations.begin(), declarations.end(),
                                              [&](const auto& d) { return d.Default.Name == saved.Name; });
            if (!declared)
                LOGWARNING(where + saved.Name + "' is saved but no longer declared; it is kept in the component.");
        }
    }
}
