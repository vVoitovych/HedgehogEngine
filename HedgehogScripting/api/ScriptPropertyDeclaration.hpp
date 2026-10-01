#pragma once

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include <optional>
#include <string>

namespace HedgehogScripting
{
    // One entry of a script's `Properties` table, as the inspector needs it: the property with
    // its default value, and the optional range and tooltip of the long form. Pure data.
    struct ScriptPropertyDeclaration
    {
        HedgehogEngine::ScriptProperty Default;
        std::optional<float>           Min; // Number only
        std::optional<float>           Max; // Number only
        std::string                    Tooltip;
    };
}
