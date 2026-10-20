#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"
#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include <array>

// Input of a GLFW window the application does not own, so it gets no callbacks from it (an ImGui
// viewport's own window): its state polled each frame and turned into the same RawInput changes the
// window's callbacks would make. Scroll cannot be polled; whoever receives the window's scroll events
// adds them with ApplyScrollEvent.
namespace HW
{
    // What one poll of a window saw. Codes are GLFW's (InputCodes.hpp).
    struct PolledWindowState
    {
        std::array<bool, KEY_COUNT>          Keys{};
        std::array<bool, MOUSE_BUTTON_COUNT> MouseButtons{};
        double                               CursorX      = 0.0; // window coordinates, y down
        double                               CursorY      = 0.0;
        bool                                 CursorInside = false;
        bool                                 Focused      = false;
    };

    // Applies a poll through RawInputEvents: focus and entering first (so losing focus releases
    // everything), then keys only while focused, mouse buttons, and the cursor while it is inside.
    // A drag that leaves the window still counts as inside until its buttons are up, so it keeps
    // moving the cursor. Call after BeginInputFrame.
    HEDGEHOG_WINDOW_API void ApplyPolledWindowState(RawInput& input, const PolledWindowState& state);

    // Polls a GLFW window (a GLFWwindow*) into a PolledWindowState; an empty state for nullptr.
    [[nodiscard]] HEDGEHOG_WINDOW_API PolledWindowState PollWindowState(void* glfwWindow);
}
