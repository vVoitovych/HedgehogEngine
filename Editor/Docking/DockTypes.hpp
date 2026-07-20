#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Editor
{
    enum class DockArea : int
    {
        Left      = 0,
        TopCenter = 1,
        Center    = 2,
        Bottom    = 3,
        Right     = 4,
        Floating  = 5,   // panel lives as a free-floating ImGui window
        Count     = 6
    };

    enum class PanelId : int
    {
        SceneHierarchy = 0,
        Inspector      = 1,
        Console        = 2,
        Count          = 3
    };

    inline constexpr int DOCK_AREA_COUNT = static_cast<int>(DockArea::Count);
    inline constexpr int PANEL_ID_COUNT  = static_cast<int>(PanelId::Count);

    inline constexpr const char* PanelName(PanelId id)
    {
        switch (id)
        {
        case PanelId::SceneHierarchy: return "Scene Hierarchy";
        case PanelId::Inspector:      return "Inspector";
        case PanelId::Console:        return "Console";
        default:                      return "Unknown";
        }
    }

    inline constexpr const char* PanelIdToString(PanelId id)
    {
        switch (id)
        {
        case PanelId::SceneHierarchy: return "scene_hierarchy";
        case PanelId::Inspector:      return "inspector";
        case PanelId::Console:        return "console";
        default:                      return "unknown";
        }
    }

    inline std::optional<PanelId> StringToPanelId(const std::string& s)
    {
        if (s == "scene_hierarchy") return PanelId::SceneHierarchy;
        if (s == "inspector")       return PanelId::Inspector;
        if (s == "console")         return PanelId::Console;
        return std::nullopt;
    }

    struct PanelPos { float x = 200.0f; float y = 200.0f; };

    struct DockLayoutState
    {
        std::array<std::vector<PanelId>, DOCK_AREA_COUNT> AreaPanels;
        std::array<int,      DOCK_AREA_COUNT>             ActiveTab{};
        std::array<bool,     PANEL_ID_COUNT>              PanelVisible{};
        std::array<PanelPos, PANEL_ID_COUNT>              FloatingPositions{};

        float LeftWidth    = 280.0f;
        float RightWidth   = 280.0f;
        float BottomHeight = 200.0f;

        void InitDefaults()
        {
            for (auto& v : AreaPanels)
                v.clear();
            ActiveTab = {};
            PanelVisible.fill(true);

            // Stagger initial float positions so panels don't stack on each other
            for (int i = 0; i < PANEL_ID_COUNT; ++i)
                FloatingPositions[i] = { 150.0f + i * 40.0f, 150.0f + i * 40.0f };

            AreaPanels[static_cast<int>(DockArea::Left)].push_back(PanelId::SceneHierarchy);
            AreaPanels[static_cast<int>(DockArea::Right)].push_back(PanelId::Inspector);
            AreaPanels[static_cast<int>(DockArea::Bottom)].push_back(PanelId::Console);
        }

        bool IsPanelInAnyArea(PanelId id) const
        {
            for (const auto& panels : AreaPanels)
                for (PanelId p : panels)
                    if (p == id)
                        return true;
            return false;
        }

        DockArea FindPanelArea(PanelId id) const
        {
            for (int i = 0; i < DOCK_AREA_COUNT; ++i)
                for (PanelId p : AreaPanels[i])
                    if (p == id)
                        return static_cast<DockArea>(i);
            return DockArea::Floating;
        }

        bool IsValid() const
        {
            // Valid if any panel is placed (docked or floating) or was intentionally hidden
            for (const auto& panels : AreaPanels)
                if (!panels.empty())
                    return true;
            // All panels might be hidden — still valid if the visible array was serialised
            for (int i = 0; i < PANEL_ID_COUNT; ++i)
                if (!PanelVisible[i])
                    return true;
            return false;
        }

        void HidePanel(PanelId id)
        {
            PanelVisible[static_cast<int>(id)] = false;
            for (auto& panels : AreaPanels)
            {
                const auto it = std::find(panels.begin(), panels.end(), id);
                if (it != panels.end())
                {
                    panels.erase(it);
                    break;
                }
            }
        }

        void ShowPanel(PanelId id)
        {
            PanelVisible[static_cast<int>(id)] = true;
            if (!IsPanelInAnyArea(id))
                AreaPanels[static_cast<int>(DockArea::Floating)].push_back(id);
        }

        void MovePanel(PanelId id, DockArea toArea)
        {
            for (auto& panels : AreaPanels)
            {
                const auto it = std::find(panels.begin(), panels.end(), id);
                if (it != panels.end())
                {
                    panels.erase(it);
                    break;
                }
            }
            AreaPanels[static_cast<int>(toArea)].push_back(id);
        }
    };
}
