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
        Count     = 5
    };

    enum class PanelId : int
    {
        SceneHierarchy = 0,
        Inspector      = 1,
        Console        = 2,
        Count          = 3
    };

    inline constexpr int DOCK_AREA_COUNT  = static_cast<int>(DockArea::Count);
    inline constexpr int PANEL_ID_COUNT   = static_cast<int>(PanelId::Count);

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

    struct DockLayoutState
    {
        std::array<std::vector<PanelId>, DOCK_AREA_COUNT> m_AreaPanels;
        std::array<int, DOCK_AREA_COUNT>                  m_ActiveTab{};

        float m_LeftWidth    = 280.0f;
        float m_RightWidth   = 280.0f;
        float m_BottomHeight = 200.0f;

        void InitDefaults()
        {
            for (auto& v : m_AreaPanels)
                v.clear();
            m_ActiveTab = {};
            m_AreaPanels[static_cast<int>(DockArea::Left)].push_back(PanelId::SceneHierarchy);
            m_AreaPanels[static_cast<int>(DockArea::Right)].push_back(PanelId::Inspector);
            m_AreaPanels[static_cast<int>(DockArea::Bottom)].push_back(PanelId::Console);
        }

        DockArea FindPanelArea(PanelId id) const
        {
            for (int i = 0; i < DOCK_AREA_COUNT; ++i)
                for (PanelId p : m_AreaPanels[i])
                    if (p == id)
                        return static_cast<DockArea>(i);
            return DockArea::Left;
        }

        bool IsValid() const
        {
            int total = 0;
            for (const auto& panels : m_AreaPanels)
                total += static_cast<int>(panels.size());
            return total > 0;
        }

        void MovePanel(PanelId id, DockArea toArea)
        {
            for (auto& panels : m_AreaPanels)
            {
                auto it = std::find(panels.begin(), panels.end(), id);
                if (it != panels.end())
                {
                    panels.erase(it);
                    break;
                }
            }
            m_AreaPanels[static_cast<int>(toArea)].push_back(id);
        }
    };
}
