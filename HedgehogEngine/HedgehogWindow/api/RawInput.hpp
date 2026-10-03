#pragma once

#include "HedgehogEngine/HedgehogWindow/api/InputCodes.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <array>

namespace HW
{
    // The first connected gamepad, as polled this frame. All zero while none is connected.
    struct GamepadState
    {
        bool                                    Connected = false;
        std::array<bool, GAMEPAD_BUTTON_COUNT>  Buttons{};
        std::array<float, GAMEPAD_AXIS_COUNT>   Axes{};
    };

    // Everything a window's input devices report, as one plain snapshot: which keys and mouse
    // buttons are down now, where the cursor is, and how far it and the scroll wheel moved since the
    // frame began (RawInputEvents.hpp). Filled by the window's GLFW callbacks, or by a test directly.
    struct RawInput
    {
        std::array<bool, KEY_COUNT>          Keys{};
        std::array<bool, MOUSE_BUTTON_COUNT> MouseButtons{};

        HM::Vector2 CursorPosition = HM::Vector2(0.0f, 0.0f); // window coordinates, y down
        HM::Vector2 CursorDelta    = HM::Vector2(0.0f, 0.0f); // since the frame began
        HM::Vector2 ScrollDelta    = HM::Vector2(0.0f, 0.0f); // since the frame began

        GamepadState Gamepad;

        bool CursorInside = false; // over the window's client area
        bool Focused      = true;  // the window has keyboard focus

        // Whether CursorPosition holds a real position, so the next cursor event can give a delta.
        // False until the first event, and again after the cursor leaves the window.
        bool CursorKnown = false;
    };

    [[nodiscard]] inline bool IsKeyDown(const RawInput& input, Key key)
    {
        return input.Keys[static_cast<size_t>(key)];
    }

    [[nodiscard]] inline bool IsMouseButtonDown(const RawInput& input, MouseButton button)
    {
        return input.MouseButtons[static_cast<size_t>(button)];
    }
}
