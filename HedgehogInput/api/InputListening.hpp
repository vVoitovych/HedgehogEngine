#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include <cstdint>
#include <optional>

// Finding the control a user pressed next, for screens that rebind input.
namespace HInput
{
    // A control that went down between two snapshots.
    struct InputPress
    {
        uint16_t Code      = 0;    // HW::Key, HW::MouseButton, HW::GamepadButton or HW::GamepadAxis, by source
        float    Direction = 1.0f; // a gamepad axis pushed negative (a stick up or left) is -1, else +1
    };

    // The first control of source's kind, in code order, down in current and not in previous: Key and
    // KeyAxis a key with a name (GetKeyName), MouseButton and GamepadButton a button, and GamepadAxis an
    // axis whose magnitude reached PRESS_THRESHOLD and was below it before (a held axis gives nothing).
    // nullopt for pointer and scroll sources, and when nothing new is down. Escape is a key like any other, so a
    // caller that cancels on it checks for it first.
    [[nodiscard]] std::optional<InputPress> FindNewPress(const HW::RawInput& previous,
                                                         const HW::RawInput& current,
                                                         BindingSource       source);
}
