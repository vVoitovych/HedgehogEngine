#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include "ECS/api/Entity.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace HedgehogEngine
{
    // What a script property holds, and so which alternative of ScriptPropertyValue it uses:
    // Number float, Bool bool, Vector3 and Color HM::Vector3 (Color as RGB), String std::string,
    // EntityRef ECS::Entity (the scene's entity id), AssetRef std::string (a path under assets://).
    enum class ScriptPropertyType
    {
        Number,
        Bool,
        Vector3,
        Color,
        String,
        EntityRef,
        AssetRef
    };

    using ScriptPropertyValue = std::variant<bool, float, HM::Vector3, std::string, ECS::Entity>;

    // One value a script exposes to the inspector. AssetType names the content type an AssetRef
    // takes ("Mesh", "Texture", ...); it is empty for every other type.
    struct ScriptProperty
    {
        std::string         Name;
        ScriptPropertyType  Type = ScriptPropertyType::Number;
        ScriptPropertyValue Value = 0.0f;
        std::string         AssetType;
    };

    // Which script an entity runs, whether it is enabled, and its property values, in order.
    // Data only: the script system that runs it keeps all runtime state.
    struct ScriptComponent
    {
        bool                        Enable = true;
        std::string                 ScriptPath;
        std::vector<ScriptProperty> Properties;

        // Visit serializes the simple fields; RegisterScriptComponentSerializer writes Properties.
        template<typename V>
        void Visit(V& v)
        {
            v("ScriptEnable", Enable);
            v("ScriptFile",   ScriptPath);
        }
    };

    // The property called name, or nullptr.
    [[nodiscard]] inline ScriptProperty* FindScriptProperty(ScriptComponent& component, std::string_view name)
    {
        const auto it = std::find_if(component.Properties.begin(), component.Properties.end(),
                                     [name](const ScriptProperty& property) { return property.Name == name; });
        return it == component.Properties.end() ? nullptr : &*it;
    }

    [[nodiscard]] inline const ScriptProperty* FindScriptProperty(const ScriptComponent& component, std::string_view name)
    {
        const auto it = std::find_if(component.Properties.begin(), component.Properties.end(),
                                     [name](const ScriptProperty& property) { return property.Name == name; });
        return it == component.Properties.end() ? nullptr : &*it;
    }

    // Replaces the property with the same name, or appends it.
    inline void SetScriptProperty(ScriptComponent& component, ScriptProperty property)
    {
        if (ScriptProperty* existing = FindScriptProperty(component, property.Name))
            *existing = std::move(property);
        else
            component.Properties.push_back(std::move(property));
    }
}
