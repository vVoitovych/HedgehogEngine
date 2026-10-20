#pragma once

#include <optional>
#include <span>

// Keeping the editor's windows on screen: a window saved on a monitor that is gone (unplugged, or
// a layout from another machine) reopens on the main window's monitor. ImGui-free, so EditorTest
// covers it; EditorGui applies it to every window the frame it appears.
namespace Editor
{
    // A rectangle on the desktop, in pixels, y down: a window, or a monitor's work area.
    struct ScreenRect
    {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 0.0f;
        float Height = 0.0f;
    };

    // How much of a window's top edge (where its title bar is) a monitor must show for the window
    // to count as on screen: enough to grab it.
    inline constexpr float MIN_VISIBLE_WIDTH  = 32.0f;
    inline constexpr float MIN_VISIBLE_HEIGHT = 16.0f;

    // Whether some monitor shows at least MIN_VISIBLE_WIDTH x MIN_VISIBLE_HEIGHT of the window's top
    // MIN_VISIBLE_HEIGHT pixels.
    [[nodiscard]] bool IsOnAnyMonitor(const ScreenRect& window, std::span<const ScreenRect> monitors);

    // The window moved as little as it takes to lie inside monitor, size kept; a window larger than
    // the monitor goes to its top-left corner.
    [[nodiscard]] ScreenRect MoveOntoMonitor(const ScreenRect& window, const ScreenRect& monitor);

    // Where the window must go: nowhere (nullopt) when a monitor shows it, else onto fallback (the
    // main window's monitor).
    [[nodiscard]] std::optional<ScreenRect> FindPlaceOnScreen(const ScreenRect& window,
                                                              std::span<const ScreenRect> monitors,
                                                              const ScreenRect& fallback);
}
