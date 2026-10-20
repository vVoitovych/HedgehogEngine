#include "WindowPlacement.hpp"

#include <algorithm>

namespace Editor
{
    bool IsOnAnyMonitor(const ScreenRect& window, std::span<const ScreenRect> monitors)
    {
        // The window's top strip: its title bar is what the user grabs it by.
        const float top    = window.Y;
        const float bottom = window.Y + std::min(window.Height, MIN_VISIBLE_HEIGHT);
        for (const ScreenRect& monitor : monitors)
        {
            const float width  = std::min(window.X + window.Width, monitor.X + monitor.Width) - std::max(window.X, monitor.X);
            const float height = std::min(bottom, monitor.Y + monitor.Height) - std::max(top, monitor.Y);
            if (width >= MIN_VISIBLE_WIDTH && height >= std::min(window.Height, MIN_VISIBLE_HEIGHT))
                return true;
        }
        return false;
    }

    ScreenRect MoveOntoMonitor(const ScreenRect& window, const ScreenRect& monitor)
    {
        ScreenRect moved = window;
        moved.X = std::clamp(window.X, monitor.X, std::max(monitor.X, monitor.X + monitor.Width - window.Width));
        moved.Y = std::clamp(window.Y, monitor.Y, std::max(monitor.Y, monitor.Y + monitor.Height - window.Height));
        return moved;
    }

    std::optional<ScreenRect> FindPlaceOnScreen(const ScreenRect& window, std::span<const ScreenRect> monitors,
                                                const ScreenRect& fallback)
    {
        if (IsOnAnyMonitor(window, monitors))
            return std::nullopt;
        return MoveOntoMonitor(window, fallback);
    }
}
