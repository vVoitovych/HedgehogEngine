#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"

namespace HW
{
    void BeginInputFrame(RawInput& input)
    {
        input.CursorDelta = HM::Vector2(0.0f, 0.0f);
        input.ScrollDelta = HM::Vector2(0.0f, 0.0f);
    }

    void ApplyKeyEvent(RawInput& input, int key, bool down)
    {
        if (key < 0 || static_cast<size_t>(key) >= KEY_COUNT)
            return;
        input.Keys[static_cast<size_t>(key)] = down;
    }

    void ApplyMouseButtonEvent(RawInput& input, int button, bool down)
    {
        if (button < 0 || static_cast<size_t>(button) >= MOUSE_BUTTON_COUNT)
            return;
        input.MouseButtons[static_cast<size_t>(button)] = down;
    }

    void ApplyCursorEvent(RawInput& input, double x, double y)
    {
        const HM::Vector2 position(static_cast<float>(x), static_cast<float>(y));
        if (input.CursorKnown)
            input.CursorDelta = input.CursorDelta + (position - input.CursorPosition);
        input.CursorPosition = position;
        input.CursorKnown    = true;
    }

    void ApplyScrollEvent(RawInput& input, double x, double y)
    {
        input.ScrollDelta = input.ScrollDelta + HM::Vector2(static_cast<float>(x), static_cast<float>(y));
    }

    void ApplyCursorEnter(RawInput& input, bool entered)
    {
        input.CursorInside = entered;
        if (!entered)
            input.CursorKnown = false;
    }

    void ApplyFocus(RawInput& input, bool focused)
    {
        input.Focused = focused;
        if (focused)
            return;
        input.Keys.fill(false);
        input.MouseButtons.fill(false);
    }
}
