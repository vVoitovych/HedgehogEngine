#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include <cstddef>
#include <string_view>

// The player's own bindings. The overrides are an InputActionMap of Game actions, stored in the actions
// file format (WriteInputActions with the map as Game; read back as ParseInputActions(...).Game, so an
// Editor section in a file is not used).
namespace HInput
{
    // project with each Game action named in overrides given the override's bindings (an empty list
    // leaves the action unbound); every other action, the project's action order and the Editor map are
    // the project's. An override naming no Game action of the project is dropped with one "[Input] ..."
    // warning.
    [[nodiscard]] InputActionSet ApplyInputOverrides(const InputActionSet& project, const InputActionMap& overrides);

    // Sets binding bindingIndex of action in overrides. An action not overridden yet is first copied from
    // project's Game map (appended to overrides). false, changing nothing, when project's Game map has no
    // such action or bindingIndex is past the action's bindings (no append).
    [[nodiscard]] bool SetActionBinding(InputActionMap&       overrides,
                                        const InputActionMap& project,
                                        std::string_view      action,
                                        size_t                bindingIndex,
                                        const InputBinding&   binding);

    // Removes action's override, so the project's bindings apply again. Whether there was one.
    [[nodiscard]] bool ResetActionBindings(InputActionMap& overrides, std::string_view action);
}
