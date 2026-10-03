#include "DockSystem.hpp"

#include "EditorTheme.hpp"

#include <algorithm>
#include <string>

namespace Editor
{
    namespace
    {
        constexpr DockArea DOCKABLE_AREAS[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };
    }

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

        // Resolve pending drag-drop before any window is rendered this frame
        if (m_DraggingPanel.has_value() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            const DockArea target = HitTestAreas(io.MousePos, display, menuBarHeight);
            if (target != m_DraggingFromArea)
                m_Layout.MovePanel(m_DraggingPanel.value(), target);
            m_DraggingPanel.reset();
        }

        const DockGeometry geometry = ComputeDockGeometry(m_Layout, display.x, display.y, menuBarHeight);

        // Children are placed in the host window, which starts under the menu bar.
        const auto place = [menuBarHeight](const DockRect& rect)
        {
            ImGui::SetCursorPos({ rect.X, rect.Y - menuBarHeight });
            return ImVec2(rect.Width, rect.Height);
        };

        // ── Full-workspace host window ────────────────────────────────────────
        ImGui::SetNextWindowPos({ 0.0f, menuBarHeight }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ display.x, std::max(display.y - menuBarHeight, 0.0f) }, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   { 0.0f, 0.0f });
        constexpr ImGuiWindowFlags HOST_FLAGS =
            ImGuiWindowFlags_NoTitleBar        | ImGuiWindowFlags_NoResize          |
            ImGuiWindowFlags_NoMove            | ImGuiWindowFlags_NoScrollbar       |
            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings   |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;
        ImGui::Begin("##DockHost", nullptr, HOST_FLAGS);
        ImGui::PopStyleVar(2);

        // ── Toolbar row, full width ──────────────────────────────────────────
        {
            const ImVec2 size = place(geometry.Toolbar);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 4.0f });
            ImGui::BeginChild("##AreaToolbar", size, ImGuiChildFlags_None);
            drawToolbar();
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }

        DrawDockedArea(DockArea::Left, "##AreaLeft", place(geometry.Left), drawFn);

        // ── Scene view ───────────────────────────────────────────────────────
        {
            const ImVec2 size = place(geometry.Center);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::MAIN_BG);
            ImGui::BeginChild("##AreaCenter", size, ImGuiChildFlags_None);
            drawFn(PanelId::Count); // sentinel: scene view
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        DrawDockedArea(DockArea::Bottom, "##AreaBottom", place(geometry.Bottom), drawFn);
        DrawDockedArea(DockArea::Right, "##AreaRight", place(geometry.Right), drawFn);

        // ── Splitters: a drag starts from the size as laid out, so a clamped size never drifts,
        // and the saved sizes change only when dragged.
        place(geometry.LeftSplitter);
        if (const float delta = DrawSplitter("##VSplitL", geometry.LeftSplitter, false); delta != 0.0f)
            m_Layout.LeftWidth = geometry.Left.Width + delta;
        place(geometry.BottomSplitter);
        if (const float delta = DrawSplitter("##HSplitB", geometry.BottomSplitter, true); delta != 0.0f)
            m_Layout.BottomHeight = geometry.Bottom.Height - delta;
        place(geometry.RightSplitter);
        if (const float delta = DrawSplitter("##VSplitR", geometry.RightSplitter, false); delta != 0.0f)
            m_Layout.RightWidth = geometry.Right.Width - delta;

        ImGui::End(); // ##DockHost

        // ── Floating panels (rendered above the host) ─────────────────────────
        DrawFloatingPanels(drawFn);

        // ── Drop zone overlays (on top of everything) ─────────────────────────
        if (m_DraggingPanel.has_value())
            DrawDropZones(display, menuBarHeight);
    }

    // ─── Private ─────────────────────────────────────────────────────────────

    void DockSystem::DrawDockedArea(DockArea area, const char* id, ImVec2 size, const DrawFn& drawFn)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 4.0f, 4.0f });
        ImGui::BeginChild(id, size, ImGuiChildFlags_None);
        DrawDockableArea(area, size, drawFn);
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }

    // Draws a splitter at the cursor; returns how far it was dragged this frame along its axis.
    float DockSystem::DrawSplitter(const char* id, const DockRect& rect, bool horizontal)
    {
        ImGui::InvisibleButton(id, { std::max(rect.Width, 1.0f), std::max(rect.Height, 1.0f) });
        const bool active = ImGui::IsItemActive();
        if (ImGui::IsItemHovered() || active)
            ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                  ImGui::GetColorU32(ImGuiCol_Separator));
        if (!active)
            return 0.0f;
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        return horizontal ? delta.y : delta.x;
    }

    void DockSystem::DrawDockableArea(DockArea area, ImVec2 size, const DrawFn& drawFn)
    {
        const auto& panels = m_Layout.AreaPanels[static_cast<int>(area)];
        if (panels.empty())
            return;

        if (panels.size() == 1)
        {
            const PanelId pid      = panels[0];
            const ImVec2  titleSz  = { size.x, ImGui::GetFrameHeight() };

            ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
            ImGui::Button(PanelName(pid), titleSz);
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
                const bool open = ImGui::BeginTabItem(PanelName(pid));

                if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                {
                    m_DraggingPanel    = pid;
                    m_DraggingFromArea = area;
                }

                if (open)
                {
                    drawFn(pid);
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }

    void DockSystem::DrawFloatingPanels(const DrawFn& drawFn)
    {
        auto& floatingPanels = m_Layout.AreaPanels[static_cast<int>(DockArea::Floating)];

        // Iterate backwards so erasing at index i doesn't affect lower indices
        for (int i = static_cast<int>(floatingPanels.size()) - 1; i >= 0; --i)
        {
            const PanelId pid    = floatingPanels[i];
            auto&         pos    = m_Layout.FloatingPositions[static_cast<int>(pid)];

            ImGui::SetNextWindowPos({ pos.x, pos.y }, ImGuiCond_Appearing);
            ImGui::SetNextWindowSize({ 420.0f, 320.0f }, ImGuiCond_Appearing);

            bool open = true;
            const bool visible = ImGui::Begin(PanelName(pid), &open,
                ImGuiWindowFlags_NoSavedSettings);

            // Track position: window only moves when user drags the title bar
            const ImVec2 newPos    = ImGui::GetWindowPos();
            const bool   posChanged = (newPos.x != pos.x || newPos.y != pos.y);
            pos.x = newPos.x;
            pos.y = newPos.y;

            if (posChanged && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_DraggingPanel    = pid;
                m_DraggingFromArea = DockArea::Floating;
            }

            if (visible)
                drawFn(pid);

            ImGui::End();

            if (!open)
                m_Layout.HidePanel(pid);
        }
    }

    void DockSystem::DrawDropZones(ImVec2 display, float menuH)
    {
        if (!m_DraggingPanel.has_value())
            return;

        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        ImGui::GetForegroundDrawList()->AddText(
            { mousePos.x + 14.0f, mousePos.y },
            ImGui::GetColorU32(Theme::TEXT),
            PanelName(m_DraggingPanel.value()));

        const DockGeometry geometry = ComputeDockGeometry(m_Layout, display.x, display.y, menuH);
        for (const DockArea targetArea : DOCKABLE_AREAS)
        {
            // Don't highlight the area the panel is already docked in
            if (targetArea == m_DraggingFromArea)
                continue;

            const DockRect rect    = GetAreaRect(geometry, targetArea);
            const ImVec2   bMin    = { rect.X, rect.Y };
            const ImVec2   bMax    = { rect.X + rect.Width, rect.Y + rect.Height };
            const bool     hovered = IsPointInRect(mousePos, bMin, bMax);

            const ImU32 fill    = ImGui::GetColorU32(hovered ? Theme::ACCENT_FILL : Theme::WithAlpha(Theme::TEXT, 0.06f));
            const ImU32 outline = ImGui::GetColorU32(hovered ? Theme::ACCENT : Theme::WithAlpha(Theme::TEXT_MUTED, 0.5f));

            ImGui::GetForegroundDrawList()->AddRectFilled(bMin, bMax, fill, 4.0f);
            ImGui::GetForegroundDrawList()->AddRect(bMin, bMax, outline, 4.0f, 0, 2.0f);
        }
    }

    DockArea DockSystem::HitTestAreas(ImVec2 mouse, ImVec2 display, float menuH) const
    {
        const DockGeometry geometry = ComputeDockGeometry(m_Layout, display.x, display.y, menuH);
        for (const DockArea area : DOCKABLE_AREAS)
        {
            const DockRect rect = GetAreaRect(geometry, area);
            if (IsPointInRect(mouse, { rect.X, rect.Y }, { rect.X + rect.Width, rect.Y + rect.Height }))
                return area;
        }
        // Not over any dock area — release goes to floating
        return DockArea::Floating;
    }

    bool DockSystem::IsPointInRect(ImVec2 point, ImVec2 rectMin, ImVec2 rectMax)
    {
        return point.x >= rectMin.x && point.x <= rectMax.x &&
               point.y >= rectMin.y && point.y <= rectMax.y;
    }
}
