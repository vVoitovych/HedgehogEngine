#pragma once

#include "DockTypes.hpp"

#include "imgui.h"

#include <functional>
#include <optional>

namespace Editor
{
    // Manages dockable panel layout: area sizing, tab bars, drag-to-dock.
    // Call Draw() each ImGui frame; it renders the full workspace as a child-window
    // tree and invokes drawFn for each visible panel.
    class DockSystem
    {
    public:
        using DrawFn = std::function<void(PanelId)>;

        DockSystem();

        DockSystem(const DockSystem&)            = delete;
        DockSystem& operator=(const DockSystem&) = delete;
        DockSystem(DockSystem&&)                 = delete;
        DockSystem& operator=(DockSystem&&)      = delete;

        // Renders the workspace. menuBarHeight is the pixel height of the main menu bar.
        // drawToolbar and drawFn supply content for the toolbar and each dockable panel.
        void Draw(const std::function<void()>& drawToolbar,
                  const DrawFn& drawFn,
                  float menuBarHeight);

        DockLayoutState&       GetLayout()       { return m_Layout; }
        const DockLayoutState& GetLayout() const { return m_Layout; }

    private:
        struct AreaBounds { ImVec2 m_Pos; ImVec2 m_Size; };

        AreaBounds ComputeBounds(DockArea area, ImVec2 display, float menuH) const;

        void DrawDockableArea(DockArea area, ImVec2 size, const DrawFn& drawFn);
        void DrawFloatingPanels(const DrawFn& drawFn);
        void DrawDropZones(ImVec2 display, float menuH);
        DockArea HitTestAreas(ImVec2 mouse, ImVec2 display, float menuH) const;

        static bool IsPointInRect(ImVec2 point, ImVec2 rectMin, ImVec2 rectMax);

        DockLayoutState        m_Layout;
        std::optional<PanelId> m_DraggingPanel;
        DockArea               m_DraggingFromArea = DockArea::Left;

        static constexpr float k_SplitterThickness = 4.0f;
        static constexpr float k_ToolbarHeight     = 32.0f;
        static constexpr float k_MinAreaSize       = 80.0f;
        static constexpr float k_MinCenterWidth    = 200.0f;
        static constexpr float k_TabBarHeight      = 21.0f;
    };
}
