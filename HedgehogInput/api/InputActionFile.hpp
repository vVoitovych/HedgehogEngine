#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include <optional>
#include <string>
#include <string_view>

// The text form of an InputActionSet, version 1:
//
//   Version: 1
//   Game:
//     UiSubmit:
//       - Key: Enter
//       - Key: Space
//     MoveX:
//       - KeyAxis: { Negative: A, Positive: D }
//   Editor:
//     EditorCameraLookX:
//       - PointerDelta: X
//         Deadzone: 2
//
// Each action is a list of bindings in file order. A binding has exactly one source (Key,
// MouseButton, KeyAxis, PointerDelta: X|Y or Scroll: X|Y) and may add Scale and Deadzone.
namespace HInput
{
    inline constexpr int INPUT_ACTIONS_VERSION = 1;

    struct InputActionParseResult
    {
        std::optional<InputActionSet> Actions; // empty on any error
        std::string                   Error;   // one message naming the map, action and binding at fault
    };

    // Reads text strictly: an unknown top-level key, source, field or name, a duplicate action
    // (yaml-cpp refuses repeated map keys, naming the line), a binding with no source or two, a
    // missing or other Version, or malformed YAML fails. A missing
    // Game or Editor section is an empty map.
    [[nodiscard]] InputActionParseResult ParseInputActions(std::string_view text);

    // Writes set in the layout above, leaving out a Scale of 1 and a Deadzone of 0, so writing what
    // ParseInputActions read gives the same text again.
    [[nodiscard]] std::string WriteInputActions(const InputActionSet& set);
}
