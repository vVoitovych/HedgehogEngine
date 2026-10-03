#include "DockSystem.hpp"

#include "EditorTheme.hpp"
#include "Widgets/IconWidgets.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace Editor
{
    namespace
    {
        constexpr DockArea DOCKABLE_AREAS[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };

        // Spaces at the start of a tab's label, where its icon is drawn.
        constexpr const char* TAB_ICON_PAD = "      ";
    }

    DockSystem::DockSystem()
    {
        m_Layout.InitDefaults();
    }

    // ─── Public ──────────────────────────────────────────────────────────────

    void DockSystem::Draw(const std::function<void()>& drawToolbar,
                          const DrawFn& drawFn,
                          float menuBarHeight,
                          const DockPanelIcons& icons)
    {
        m_Icons = icons;

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
        DrawDockableArea(area, drawFn);
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

    // Every docked area draws a tab strip, one tab for a single panel: each tab shows its panel's
    // icon, and a button at the strip's right end opens the menu of the panel shown.
    void DockSystem::DrawDockableArea(DockArea area, const DrawFn& drawFn)
    {
        const auto& panels = m_Layout.AreaPanels[static_cast<int>(area)];
        if (panels.empty())
            return;

        // The menu button sits at the right end of the tab row, past the tabs' natural width (an
        // area holds at most every panel, so they never reach it).
        const ImVec2 rowStart    = ImGui::GetCursorPos();
        const float  buttonWidth = ICON_SIZE_SMALL + 2.0f * ImGui::GetStyle().FramePadding.x;
        const float  buttonX     = rowStart.x + std::max(ImGui::GetContentRegionAvail().x - buttonWidth, 0.0f);

        if (!ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_Reorderable))
            return;

        const ImU32 iconTint = ImGui::GetColorU32(ImGuiCol_Text);
        const auto  drawTabIcon = [iconTint](void* icon)
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            const float  y   = min.y + (ImGui::GetItemRectSize().y - ICON_SIZE_SMALL) * 0.5f;
            DrawIcon(*ImGui::GetWindowDrawList(), icon, ImVec2(min.x + ImGui::GetStyle().FramePadding.x, y),
                     ICON_SIZE_SMALL, iconTint);
        };

        std::optional<PanelId> shown;
        char                   label[64];
        for (const PanelId pid : panels)
        {
            // The label leaves room for the icon; the id after ### stays the panel's saved key.
            std::snprintf(label, sizeof(label), "%s%s###%s", TAB_ICON_PAD, PanelName(pid), PanelIdToString(pid));
            const bool open = ImGui::BeginTabItem(label);
            drawTabIcon(m_Icons.Panels[static_cast<size_t>(pid)]);

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                m_DraggingPanel    = pid;
                m_DraggingFromArea = area;
            }

            if (open)
            {
                shown = pid;
                drawFn(pid);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();

        // Drawn last in the area, so nothing after it needs the cursor back.
        ImGui::SetCursorPos(ImVec2(buttonX, rowStart.y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        const bool openMenu = IconButton("##PanelMenuButton", m_Icons.More, ICON_SIZE_SMALL, ImGui::GetStyle().Colors[ImGuiCol_Text]);
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Panel menu");

        if (openMenu)
            ImGui::OpenPopup("##PanelMenu");
        if (shown && ImGui::BeginPopup("##PanelMenu"))
        {
            DrawPanelMenu(area, *shown);
            ImGui::EndPopup();
        }
    }

    // Hide, or move the panel to another area; the area it is in is greyed.
    void DockSystem::DrawPanelMenu(DockArea area, PanelId panel)
    {
        ImGui::TextDisabled("%s", PanelName(panel));
        ImGui::Separator();
        if (ImGui::MenuItem("Hide"))
            m_Layout.HidePanel(panel);

        constexpr struct
        {
            DockArea    Area;
            const char* Label;
        } TARGETS[] = {
            { DockArea::Left,     "Move to Left" },
            { DockArea::Right,    "Move to Right" },
            { DockArea::Bottom,   "Move to Bottom" },
            { DockArea::Floating, "Float" },
        };
        for (const auto& target : TARGETS)
        {
            if (ImGui::MenuItem(target.Label, nullptr, false, target.Area != area))
                m_Layout.MovePanel(panel, target.Area);
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
