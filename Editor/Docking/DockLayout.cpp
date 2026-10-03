#include "DockLayout.hpp"

#include <algorithm>

namespace Editor
{
    namespace
    {
        // value clamped to [minimum, maximum], where a maximum below the minimum (a window too
        // small for it) wins, and nothing goes below zero.
        [[nodiscard]] float Fit(float value, float minimum, float maximum)
        {
            maximum = std::max(maximum, 0.0f);
            return std::clamp(value, std::min(minimum, maximum), maximum);
        }
    }

    DockGeometry ComputeDockGeometry(const DockLayoutState& layout, float displayWidth,
                                     float displayHeight, float menuBarHeight)
    {
        const float width     = std::max(displayWidth, 0.0f);
        const float height    = std::max(displayHeight - menuBarHeight, 0.0f);
        const float splitters = 2.0f * DOCK_SPLITTER_THICKNESS;

        DockGeometry geometry;
        geometry.Toolbar = { 0.0f, menuBarHeight, width, std::min(DOCK_TOOLBAR_HEIGHT, height) };

        const float top        = menuBarHeight + geometry.Toolbar.Height;
        const float workHeight = height - geometry.Toolbar.Height;

        const float rightWidth = Fit(layout.RightWidth, DOCK_MIN_AREA_SIZE,
                                     width - splitters - DOCK_MIN_AREA_SIZE - DOCK_MIN_CENTER_WIDTH);
        const float leftWidth  = Fit(layout.LeftWidth, DOCK_MIN_AREA_SIZE,
                                     width - splitters - rightWidth - DOCK_MIN_CENTER_WIDTH);
        const float centerWidth = std::max(width - splitters - leftWidth - rightWidth, 0.0f);

        const float bottomHeight = Fit(layout.BottomHeight, DOCK_MIN_AREA_SIZE,
                                       workHeight - DOCK_SPLITTER_THICKNESS - DOCK_MIN_AREA_SIZE);
        const float upperHeight  = std::max(workHeight - DOCK_SPLITTER_THICKNESS - bottomHeight, 0.0f);

        const float centerX     = leftWidth + DOCK_SPLITTER_THICKNESS;
        const float bottomWidth = centerX + centerWidth;
        const float bottomY     = top + upperHeight + DOCK_SPLITTER_THICKNESS;

        geometry.Left           = { 0.0f, top, leftWidth, upperHeight };
        geometry.LeftSplitter   = { leftWidth, top, DOCK_SPLITTER_THICKNESS, upperHeight };
        geometry.Center         = { centerX, top, centerWidth, upperHeight };
        geometry.BottomSplitter = { 0.0f, top + upperHeight, bottomWidth, DOCK_SPLITTER_THICKNESS };
        geometry.Bottom         = { 0.0f, bottomY, bottomWidth, bottomHeight };
        geometry.RightSplitter  = { bottomWidth, top, DOCK_SPLITTER_THICKNESS, workHeight };
        geometry.Right          = { bottomWidth + DOCK_SPLITTER_THICKNESS, top, rightWidth, workHeight };
        return geometry;
    }

    DockRect GetAreaRect(const DockGeometry& geometry, DockArea area)
    {
        switch (area)
        {
        case DockArea::Left:      return geometry.Left;
        case DockArea::TopCenter: return geometry.Toolbar;
        case DockArea::Center:    return geometry.Center;
        case DockArea::Bottom:    return geometry.Bottom;
        case DockArea::Right:     return geometry.Right;
        default:                  return {};
        }
    }
}
