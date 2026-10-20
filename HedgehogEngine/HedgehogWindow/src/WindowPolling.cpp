#include "HedgehogEngine/HedgehogWindow/api/WindowPolling.hpp"

#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"

#include "GLFW/glfw3.h"

#include <utility>

namespace HW
{
    namespace
    {
        // GLFW's key codes, as ranges: asking it for any other code is an error.
        constexpr std::pair<int, int> KEY_RANGES[] = {
            { GLFW_KEY_SPACE, GLFW_KEY_SPACE },
            { GLFW_KEY_APOSTROPHE, GLFW_KEY_APOSTROPHE },
            { GLFW_KEY_COMMA, GLFW_KEY_9 },
            { GLFW_KEY_SEMICOLON, GLFW_KEY_SEMICOLON },
            { GLFW_KEY_EQUAL, GLFW_KEY_EQUAL },
            { GLFW_KEY_A, GLFW_KEY_RIGHT_BRACKET },
            { GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_GRAVE_ACCENT },
            { GLFW_KEY_WORLD_1, GLFW_KEY_WORLD_2 },
            { GLFW_KEY_ESCAPE, GLFW_KEY_END },
            { GLFW_KEY_CAPS_LOCK, GLFW_KEY_PAUSE },
            { GLFW_KEY_F1, GLFW_KEY_F25 },
            { GLFW_KEY_KP_0, GLFW_KEY_KP_EQUAL },
            { GLFW_KEY_LEFT_SHIFT, GLFW_KEY_MENU },
        };
    }

    void ApplyPolledWindowState(RawInput& input, const PolledWindowState& state)
    {
        bool anyButton = false;
        for (const bool down : state.MouseButtons)
            anyButton = anyButton || down;

        if (state.Focused != input.Focused)
            ApplyFocus(input, state.Focused);
        // A drag that leaves the window still belongs to it: leaving counts once the buttons are up.
        const bool inside = state.CursorInside || (anyButton && input.CursorInside);
        if (inside != input.CursorInside)
            ApplyCursorEnter(input, inside);

        for (size_t key = 0; key < KEY_COUNT; ++key)
        {
            const bool down = state.Focused && state.Keys[key];
            if (down != input.Keys[key])
                ApplyKeyEvent(input, static_cast<int>(key), down);
        }
        for (size_t button = 0; button < MOUSE_BUTTON_COUNT; ++button)
        {
            const bool down = state.MouseButtons[button];
            if (down != input.MouseButtons[button])
                ApplyMouseButtonEvent(input, static_cast<int>(button), down);
        }
        if (inside)
            ApplyCursorEvent(input, state.CursorX, state.CursorY);
    }

    PolledWindowState PollWindowState(void* glfwWindow)
    {
        PolledWindowState state;
        GLFWwindow* window = static_cast<GLFWwindow*>(glfwWindow);
        if (!window)
            return state;
        for (const auto& [first, last] : KEY_RANGES)
        {
            for (int key = first; key <= last; ++key)
                state.Keys[static_cast<size_t>(key)] = glfwGetKey(window, key) == GLFW_PRESS;
        }
        for (int button = 0; button < static_cast<int>(MOUSE_BUTTON_COUNT); ++button)
            state.MouseButtons[static_cast<size_t>(button)] = glfwGetMouseButton(window, button) == GLFW_PRESS;
        glfwGetCursorPos(window, &state.CursorX, &state.CursorY);
        state.CursorInside = glfwGetWindowAttrib(window, GLFW_HOVERED) == GLFW_TRUE;
        state.Focused      = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        return state;
    }
}
