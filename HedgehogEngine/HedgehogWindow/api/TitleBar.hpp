#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"

#include <span>

// A window that draws its own title bar (Window::UseCustomTitleBar): which part of its client area
// the OS treats as what. Platform-free, so HedgehogWindowTest covers it.
namespace HW
{
    // A rectangle in the window's client coordinates (the cursor's), y down.
    struct ClientRect
    {
        int X      = 0;
        int Y      = 0;
        int Width  = 0;
        int Height = 0;
    };

    enum class TitleBarHit
    {
        Client,  // the application's: menus, buttons, everything else
        Caption, // drags the window, a double click maximizes or restores it
        Top      // resizes from the top edge
    };

    // A point within topBorder of the top edge resizes, unless the window is maximized; else a point
    // in a caption area drags; else it is the application's.
    [[nodiscard]] HEDGEHOG_WINDOW_API TitleBarHit HitTestTitleBar(int x, int y, bool maximized, int topBorder,
                                                                  std::span<const ClientRect> captionAreas);
}
