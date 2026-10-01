#pragma once

#include "HedgehogScripting/api/ScriptPropertyDeclaration.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include <vector>

namespace ECS
{
    class ECS;
}

namespace Editor
{
    // The script inspector's Properties section: one field per declared property, in the order
    // the declarations come (by name). A field shows the component's saved value when it has one
    // of the declared type, else the declared default, and writes the component only when edited.
    // Numbers drag within the declared min and max, colours use a colour picker, an EntityRef
    // takes an entity dragged from the hierarchy and an AssetRef an asset of its declared type
    // from the Content panel. Every field shows the declaration's tooltip and, once it differs
    // from the default, a Reset button. Saved values the script no longer declares are listed
    // read-only. Without declarations (the script could not be described) the saved values are
    // drawn as they are, with no range, tooltip or reset. Returns true when a value changed.
    [[nodiscard]] bool DrawScriptProperties(HedgehogEngine::ScriptComponent&                             component,
                                            const std::vector<HedgehogScripting::ScriptPropertyDeclaration>* declarations,
                                            const ECS::ECS&                                              ecs);
}
