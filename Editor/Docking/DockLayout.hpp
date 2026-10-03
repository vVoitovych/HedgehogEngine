#pragma once

#include "DockTypes.hpp"

namespace Editor
{
    inline constexpr float DOCK_SPLITTER_THICKNESS = 4.0f;
    inline constexpr float DOCK_TOOLBAR_HEIGHT     = 32.0f;
    inline constexpr float DOCK_MIN_AREA_SIZE      = 80.0f;
    inline constexpr float DOCK_MIN_CENTER_WIDTH   = 200.0f;

    // A rectangle in screen pixels, y down.
    struct DockRect
    {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 0.0f;
        float Height = 0.0f;
    };

    // Where every area and splitter of the workspace sits. The toolbar runs the full width under
    // the menu bar; the Inspector's Right area is full height below it; Left and Center share the
    // upper band, and Bottom spans under both of them.
    struct DockGeometry
    {
        DockRect Toolbar;
        DockRect Left;
        DockRect Center;
        DockRect Bottom;
        DockRect Right;
        DockRect LeftSplitter;   // between Left and Center, upper band only
        DockRect BottomSplitter; // above Bottom, across Left and Center
        DockRect RightSplitter;  // left of Right, full height below the toolbar
    };

    // Lays the workspace out from the layout's saved sizes, clamped so Center keeps
    // DOCK_MIN_CENTER_WIDTH and every area DOCK_MIN_AREA_SIZE where the window allows; no size is
    // ever negative.
    [[nodiscard]] DockGeometry ComputeDockGeometry(const DockLayoutState& layout, float displayWidth,
                                                   float displayHeight, float menuBarHeight);

    // The geometry's rectangle for a docked area; an empty one for Floating.
    [[nodiscard]] DockRect GetAreaRect(const DockGeometry& geometry, DockArea area);
}
