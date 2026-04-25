#include "DockSystem.hpp"

#include <algorithm>
#include <string>

namespace Editor
{
    DockSystem::DockSystem()
    {
        m_Layout.InitDefaults();
    }

    // ─── Public ──────────────────────────────────────────────────────────────

    void DockSystem::Draw(const std::function<void()>& drawToolbar,
                          const DrawFn& drawFn,
                          float menuBarHeight)
    {
        const ImGuiIO& io      = ImGui::GetIO();
        const ImVec2   display = io.DisplaySize;
        const float    H       = display.y - menuBarHeight;

        // Handle drag completion before drawing (avoids one-frame flicker)
        if (m_DraggingPanel.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            const DockArea target = HitTestAreas(io.MousePos, display, menuBarHeight);
            // Only move to dockable areas
            if (target == DockArea::Left || target == DockArea::Right || target == DockArea::Bottom)
                m_Layout.MovePanel(m_DraggingPanel.value(), target);
            m_DraggingPanel.reset();
        }

        const float leftW   = m_Layout.m_LeftWidth;
        const float rightW  = m_Layout.m_RightWidth;
        const float centerW = std::max(k_MinCenterWidth,
                                       display.x - leftW - rightW - 2.0f * k_SplitterThickness);
        const float bottomH = m_Layout.m_BottomHeight;
        const float sceneH  = std::max(k_MinAreaSize, H - k_ToolbarHeight - bottomH - k_SplitterThickness);

        // ── Full-workspace host window ────────────────────────────────────────
        ImGui::SetNextWindowPos({ 0.0f, menuBarHeight }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ display.x, H }, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   { 0.0f, 0.0f });
        constexpr ImGuiWindowFlags k_HostFlags =
            ImGuiWindowFlags_NoTitleBar        | ImGuiWindowFlags_NoResize         |
            ImGuiWindowFlags_NoMove            | ImGuiWindowFlags_NoScrollbar      |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings  |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("##DockHost", nullptr, k_HostFlags);
        ImGui::PopStyleVar(2);

        // ── LEFT AREA ────────────────────────────────────────────────────────
        ImGui::SetCursorPos({ 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });
        ImGui::BeginChild("##AreaLeft", { leftW, H }, ImGuiChildFlags_None);
        DrawDockableArea(DockArea::Left, { leftW, H }, drawFn);
        ImGui::EndChild();
        ImGui::PopStyleVar();

        // ── VERTICAL SPLITTER left/center ────────────────────────────────────
        ImGui::SetCursorPos({ leftW, 0.0f });
        ImGui::InvisibleButton("##VSplitL", { k_SplitterThickness, H });
        if (ImGui::IsItemActive())
        {
            m_Layout.m_LeftWidth = std::clamp(
                m_Layout.m_LeftWidth + io.MouseDelta.x,
                k_MinAreaSize,
                display.x - m_Layout.m_RightWidth - k_MinCenterWidth - 2.0f * k_SplitterThickness);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        {
            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();
            const ImU32  col = ImGui::GetColorU32(ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, col);
        }

        // ── CENTER COLUMN ────────────────────────────────────────────────────
        const float centerX = leftW + k_SplitterThickness;

        // Toolbar
        ImGui::SetCursorPos({ centerX, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 4.0f });
        ImGui::BeginChild("##AreaToolbar", { centerW, k_ToolbarHeight }, ImGuiChildFlags_None);
        drawToolbar();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        // Scene view
        ImGui::SetCursorPos({ centerX, k_ToolbarHeight });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::BeginChild("##AreaCenter", { centerW, sceneH }, ImGuiChildFlags_None);
        drawFn(PanelId::Count); // sentinel: scene view content
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // Horizontal splitter center/bottom
        ImGui::SetCursorPos({ centerX, k_ToolbarHeight + sceneH });
        ImGui::InvisibleButton("##HSplitB", { centerW, k_SplitterThickness });
        if (ImGui::IsItemActive())
        {
            m_Layout.m_BottomHeight = std::clamp(
                m_Layout.m_BottomHeight - io.MouseDelta.y,
                k_MinAreaSize,
                H - k_ToolbarHeight - k_MinAreaSize - k_SplitterThickness);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        {
            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_Separator));
        }

        // Bottom area
        const float bottomY = k_ToolbarHeight + sceneH + k_SplitterThickness;
        const float actualBottomH = H - bottomY;
        ImGui::SetCursorPos({ centerX, bottomY });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });
        ImGui::BeginChild("##AreaBottom", { centerW, actualBottomH }, ImGuiChildFlags_None);
        DrawDockableArea(DockArea::Bottom, { centerW, actualBottomH }, drawFn);
        ImGui::EndChild();
        ImGui::PopStyleVar();

        // ── VERTICAL SPLITTER center/right ───────────────────────────────────
        const float vSplit2X = centerX + centerW;
        ImGui::SetCursorPos({ vSplit2X, 0.0f });
        ImGui::InvisibleButton("##VSplitR", { k_SplitterThickness, H });
        if (ImGui::IsItemActive())
        {
            m_Layout.m_RightWidth = std::clamp(
                m_Layout.m_RightWidth - io.MouseDelta.x,
                k_MinAreaSize,
                display.x - m_Layout.m_LeftWidth - k_MinCenterWidth - 2.0f * k_SplitterThickness);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        {
            const ImVec2 p0 = ImGui::GetItemRectMin();
            const ImVec2 p1 = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_Separator));
        }

        // ── RIGHT AREA ───────────────────────────────────────────────────────
        ImGui::SetCursorPos({ vSplit2X + k_SplitterThickness, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });
        ImGui::BeginChild("##AreaRight", { rightW, H }, ImGuiChildFlags_None);
        DrawDockableArea(DockArea::Right, { rightW, H }, drawFn);
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End(); // ##DockHost

        // ── Drop zone overlay (drawn over everything) ─────────────────────────
        if (m_DraggingPanel.has_value())
            DrawDropZones(display, menuBarHeight);
    }

    // ─── Private ─────────────────────────────────────────────────────────────

    DockSystem::AreaBounds DockSystem::ComputeBounds(DockArea area, ImVec2 display, float menuH) const
    {
        const float H       = display.y - menuH;
        const float leftW   = m_Layout.m_LeftWidth;
        const float rightW  = m_Layout.m_RightWidth;
        const float centerW = std::max(k_MinCenterWidth,
                                       display.x - leftW - rightW - 2.0f * k_SplitterThickness);
        const float centerX = leftW + k_SplitterThickness;
        const float bottomH = m_Layout.m_BottomHeight;
        const float sceneH  = std::max(k_MinAreaSize, H - k_ToolbarHeight - bottomH - k_SplitterThickness);
        const float bottomY = menuH + k_ToolbarHeight + sceneH + k_SplitterThickness;

        switch (area)
        {
        case DockArea::Left:
            return { { 0.0f, menuH }, { leftW, H } };
        case DockArea::TopCenter:
            return { { centerX, menuH }, { centerW, k_ToolbarHeight } };
        case DockArea::Center:
            return { { centerX, menuH + k_ToolbarHeight }, { centerW, sceneH } };
        case DockArea::Bottom:
            return { { centerX, bottomY }, { centerW, H - bottomY + menuH } };
        case DockArea::Right:
            return { { centerX + centerW + k_SplitterThickness, menuH }, { rightW, H } };
        default:
            return { { 0.0f, menuH }, { 0.0f, 0.0f } };
        }
    }

    void DockSystem::DrawDockableArea(DockArea area, ImVec2 size, const DrawFn& drawFn)
    {
        auto& panels = m_Layout.m_AreaPanels[static_cast<int>(area)];
        if (panels.empty())
            return;

        if (panels.size() == 1)
        {
            // Single panel: show a slim draggable title bar then the content
            const PanelId   pid  = panels[0];
            const char*     name = PanelName(pid);
            const ImVec2    titleSize { size.x, k_TabBarHeight };

            // Title bar button acts as drag handle
            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
            ImGui::Button(name, titleSize);
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                m_DraggingPanel    = pid;
                m_DraggingFromArea = area;
            }

            drawFn(pid);
            return;
        }

        // Multiple panels: tab bar
        if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_Reorderable))
        {
            for (PanelId pid : panels)
            {
                const bool visible = ImGui::BeginTabItem(PanelName(pid));

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    m_DraggingPanel    = pid;
                    m_DraggingFromArea = area;
                }

                if (visible)
                {
                    drawFn(pid);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    void DockSystem::DrawDropZones(ImVec2 display, float menuH)
    {
        if (!m_DraggingPanel.has_value())
            return;

        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        ImGui::GetForegroundDrawList()->AddText(
            { mousePos.x + 14.0f, mousePos.y },
            IM_COL32(255, 255, 255, 200),
            PanelName(m_DraggingPanel.value()));

        constexpr DockArea k_DockableAreas[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };

        for (const DockArea targetArea : k_DockableAreas)
        {
            if (targetArea == m_DraggingFromArea)
                continue;

            const AreaBounds b       = ComputeBounds(targetArea, display, menuH);
            const ImVec2     bMax    = { b.m_Pos.x + b.m_Size.x, b.m_Pos.y + b.m_Size.y };
            const bool       hovered = IsPointInRect(mousePos, b.m_Pos, bMax);

            const ImU32 fill    = hovered ? IM_COL32(60, 180, 60, 100) : IM_COL32(255, 255, 255, 30);
            const ImU32 outline = hovered ? IM_COL32(60, 255, 60, 220) : IM_COL32(200, 200, 200, 80);

            ImGui::GetForegroundDrawList()->AddRectFilled(b.m_Pos, bMax, fill, 4.0f);
            ImGui::GetForegroundDrawList()->AddRect(b.m_Pos, bMax, outline, 4.0f, 0, 2.0f);
        }
    }

    DockArea DockSystem::HitTestAreas(ImVec2 mouse, ImVec2 display, float menuH) const
    {
        constexpr DockArea k_DockableAreas[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };
        for (const DockArea area : k_DockableAreas)
        {
            const AreaBounds b = ComputeBounds(area, display, menuH);
            if (IsPointInRect(mouse, b.m_Pos, { b.m_Pos.x + b.m_Size.x, b.m_Pos.y + b.m_Size.y }))
                return area;
        }
        return m_DraggingFromArea;
    }

    bool DockSystem::IsPointInRect(ImVec2 point, ImVec2 rectMin, ImVec2 rectMax)
    {
        return point.x >= rectMin.x && point.x <= rectMax.x &&
               point.y >= rectMin.y && point.y <= rectMax.y;
    }
}
