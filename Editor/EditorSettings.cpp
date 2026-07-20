#include "EditorSettings.hpp"

#include "yaml-cpp/yaml.h"

#include "Logger/api/Logger.hpp"

#include <string>

namespace Editor
{
    namespace
    {
        // Must match DockArea enum order; index 5 = Floating
        constexpr const char* k_DockAreaKeys[] =
            { "left", "top_center", "center", "bottom", "right", "floating" };

        // Areas that are fully serialised (Left/Right/Bottom/Floating)
        constexpr DockArea k_SavedAreas[] =
            { DockArea::Left, DockArea::Right, DockArea::Bottom, DockArea::Floating };
    }

    void EditorSettings::Save(const std::string& virtualPath,
                               const FS::FileSystemManager& fileSystem) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        // Panel background colour
        out << YAML::Key << "panel_bg_color" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << panelBgColor[0] << panelBgColor[1] << panelBgColor[2]
            << YAML::EndSeq;

        out << YAML::Key << "dock_layout" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "left_width"    << YAML::Value << dockLayout.LeftWidth;
        out << YAML::Key << "right_width"   << YAML::Value << dockLayout.RightWidth;
        out << YAML::Key << "bottom_height" << YAML::Value << dockLayout.BottomHeight;

        // Area → panel lists and active tab indices
        out << YAML::Key << "areas" << YAML::Value << YAML::BeginMap;
        for (const DockArea area : k_SavedAreas)
        {
            const int areaIdx = static_cast<int>(area);
            out << YAML::Key << k_DockAreaKeys[areaIdx]
                << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const PanelId pid : dockLayout.AreaPanels[areaIdx])
                out << PanelIdToString(pid);
            out << YAML::EndSeq;

            out << YAML::Key << (std::string(k_DockAreaKeys[areaIdx]) + "_active")
                << YAML::Value << dockLayout.ActiveTab[areaIdx];
        }
        out << YAML::EndMap; // areas

        // Per-panel visibility
        out << YAML::Key << "panel_visible" << YAML::Value << YAML::BeginMap;
        for (int i = 0; i < PANEL_ID_COUNT; ++i)
            out << YAML::Key << PanelIdToString(static_cast<PanelId>(i))
                << YAML::Value << dockLayout.PanelVisible[i];
        out << YAML::EndMap; // panel_visible

        // Floating panel last-known positions
        out << YAML::Key << "floating_positions" << YAML::Value << YAML::BeginMap;
        for (int i = 0; i < PANEL_ID_COUNT; ++i)
        {
            const auto& p = dockLayout.FloatingPositions[i];
            out << YAML::Key << PanelIdToString(static_cast<PanelId>(i))
                << YAML::Value << YAML::Flow << YAML::BeginSeq
                << p.x << p.y
                << YAML::EndSeq;
        }
        out << YAML::EndMap; // floating_positions

        out << YAML::EndMap; // dock_layout

        out << YAML::Key << "last_scene" << YAML::Value << LastScene;

        out << YAML::EndMap; // root

        if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
            LOGERROR("EditorSettings::Save: failed to write '", virtualPath, "'.");
    }

    bool EditorSettings::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const auto text = fileSystem.ReadTextFile(virtualPath);
        if (!text)
            return false;

        try
        {
            YAML::Node root = YAML::Load(*text);

            if (auto n = root["panel_bg_color"])
            {
                if (n.IsSequence() && n.size() == 3)
                {
                    panelBgColor[0] = n[0].as<float>();
                    panelBgColor[1] = n[1].as<float>();
                    panelBgColor[2] = n[2].as<float>();
                }
            }

            if (auto dock = root["dock_layout"])
            {
                if (auto n = dock["left_width"])    dockLayout.LeftWidth    = n.as<float>();
                if (auto n = dock["right_width"])   dockLayout.RightWidth   = n.as<float>();
                if (auto n = dock["bottom_height"]) dockLayout.BottomHeight = n.as<float>();

                if (auto areas = dock["areas"])
                {
                    for (const DockArea area : k_SavedAreas)
                    {
                        const int areaIdx = static_cast<int>(area);
                        if (auto seq = areas[k_DockAreaKeys[areaIdx]])
                        {
                            dockLayout.AreaPanels[areaIdx].clear();
                            for (const auto& node : seq)
                            {
                                if (auto pid = StringToPanelId(node.as<std::string>()))
                                    dockLayout.AreaPanels[areaIdx].push_back(pid.value());
                            }
                        }
                        const std::string activeKey =
                            std::string(k_DockAreaKeys[areaIdx]) + "_active";
                        if (auto n = areas[activeKey])
                            dockLayout.ActiveTab[areaIdx] = n.as<int>();
                    }
                }

                if (auto vis = dock["panel_visible"])
                {
                    for (int i = 0; i < PANEL_ID_COUNT; ++i)
                    {
                        const char* key = PanelIdToString(static_cast<PanelId>(i));
                        if (auto n = vis[key])
                            dockLayout.PanelVisible[i] = n.as<bool>();
                    }
                }

                if (auto fp = dock["floating_positions"])
                {
                    for (int i = 0; i < PANEL_ID_COUNT; ++i)
                    {
                        const char* key = PanelIdToString(static_cast<PanelId>(i));
                        if (auto seq = fp[key])
                        {
                            if (seq.IsSequence() && seq.size() == 2)
                            {
                                dockLayout.FloatingPositions[i].x = seq[0].as<float>();
                                dockLayout.FloatingPositions[i].y = seq[1].as<float>();
                            }
                        }
                    }
                }
            }
            else
            {
                dockLayout.InitDefaults();
            }

            if (auto n = root["last_scene"])
                LastScene = n.as<std::string>();

            return true;
        }
        catch (...) { return false; }
    }
}
