#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Named input actions and what drives them: plain data, read from and written to YAML by the
// asset layer, evaluated each frame by UpdateActionState (ActionState.hpp).
namespace HInput
{
    enum class BindingSource
    {
        Key,           // Code is an HW::Key: 1 while down
        MouseButton,   // Code is an HW::MouseButton: 1 while down
        KeyAxis,       // +1 while the Code key is down, -1 while the NegativeCode key is, 0 for both
        PointerDeltaX, // the cursor's move this frame, in pixels
        PointerDeltaY,
        ScrollX,       // the scroll wheel's move this frame
        ScrollY,
    };

    struct InputBinding
    {
        BindingSource Source       = BindingSource::Key;
        uint16_t      Code         = 0;
        uint16_t      NegativeCode = 0;    // KeyAxis only
        float         Scale        = 1.0f; // multiplies the binding's value
        float         Deadzone     = 0.0f; // pointer and scroll: a move this small or smaller counts as none

        bool operator==(const InputBinding&) const = default;
    };

    // An action is driven by any of its bindings: the one with the largest magnitude this frame wins.
    struct InputAction
    {
        std::string               Name;
        std::vector<InputBinding> Bindings;

        bool operator==(const InputAction&) const = default;
    };

    struct InputActionMap
    {
        std::vector<InputAction> Actions;

        bool operator==(const InputActionMap&) const = default;
    };

    // The project's actions: those the game reads in Play, and those the editor's own camera reads.
    struct InputActionSet
    {
        InputActionMap Game;
        InputActionMap Editor;

        bool operator==(const InputActionSet&) const = default;
    };

    // The index of the action called name (exact match), or nullopt.
    [[nodiscard]] std::optional<size_t> FindAction(const InputActionMap& map, std::string_view name);
}
