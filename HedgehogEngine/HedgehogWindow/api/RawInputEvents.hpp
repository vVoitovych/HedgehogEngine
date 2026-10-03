#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"
#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include <span>

// How input events change a RawInput, as free functions with no GLFW in them: the window's GLFW
// callbacks forward to these, and tests call them directly. Codes are GLFW's (InputCodes.hpp).
namespace HW
{
    // Starts a frame: zeroes the cursor and scroll deltas, which then sum the frame's events.
    HEDGEHOG_WINDOW_API void BeginInputFrame(RawInput& input);

    // A key went down (a press or a repeat) or up. A code outside [0, KEY_COUNT), such as GLFW's
    // unknown key (-1), changes nothing.
    HEDGEHOG_WINDOW_API void ApplyKeyEvent(RawInput& input, int key, bool down);

    // A mouse button went down or up; a button outside [0, MOUSE_BUTTON_COUNT) changes nothing.
    HEDGEHOG_WINDOW_API void ApplyMouseButtonEvent(RawInput& input, int button, bool down);

    // The cursor moved to (x, y). The move since the last known position is added to the delta; the
    // first event, and the first after the cursor left the window, only sets the position.
    HEDGEHOG_WINDOW_API void ApplyCursorEvent(RawInput& input, double x, double y);

    // The wheel scrolled; scrolls within a frame add up.
    HEDGEHOG_WINDOW_API void ApplyScrollEvent(RawInput& input, double x, double y);

    // The cursor entered or left the window's client area. Leaving forgets the position, so coming
    // back elsewhere is not a jump.
    HEDGEHOG_WINDOW_API void ApplyCursorEnter(RawInput& input, bool entered);

    // The window gained or lost keyboard focus. Losing it releases every key and mouse button, since
    // their releases go to another window.
    HEDGEHOG_WINDOW_API void ApplyFocus(RawInput& input, bool focused);

    // The first gamepad's state this frame: buttons and axes in GLFW's order (GAMEPAD_BUTTON_COUNT
    // and GAMEPAD_AXIS_COUNT of them; extra or missing entries are ignored or left zero). The
    // triggers come in GLFW's [-1, 1] and are stored as [0, 1], 0 at rest. Not connected zeroes it all.
    HEDGEHOG_WINDOW_API void ApplyGamepadState(RawInput& input, bool connected, std::span<const bool> buttons,
                                               std::span<const float> axes);
}
