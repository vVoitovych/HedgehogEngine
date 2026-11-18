#pragma once

namespace HW
{
    enum class CursorMode
    {
        Normal,   // visible and free
        Hidden,   // invisible over the window, moves freely
        Locked,   // hidden and held: its position is virtual and unbounded, only its deltas matter (mouse-look)
        Confined, // visible, kept inside the client area
    };

    // The mode a window shows: Normal while it does not have focus, else the one requested.
    [[nodiscard]] constexpr CursorMode ResolveCursorMode(CursorMode requested, bool focused) noexcept
    {
        return focused ? requested : CursorMode::Normal;
    }
}
