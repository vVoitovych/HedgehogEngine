#include "EditorSettings.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace Editor
{
    namespace
    {
        constexpr const char* k_DockAreaKeys[] = { "left", "top_center", "center", "bottom", "right" };
    }

    void EditorSettings::Save(const std::string& path) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;

        out << YAML::Key << "panel_bg_color" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << panelBgColor[0] << panelBgColor[1] << panelBgColor[2]
            << YAML::EndSeq;

        out << YAML::Key << "dock_layout" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "left_width"    << YAML::Value << dockLayout.m_LeftWidth;
        out << YAML::Key << "right_width"   << YAML::Value << dockLayout.m_RightWidth;
        out << YAML::Key << "bottom_height" << YAML::Value << dockLayout.m_BottomHeight;

        out << YAML::Key << "areas" << YAML::Value << YAML::BeginMap;
        constexpr DockArea k_SavedAreas[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };
        for (const DockArea area : k_SavedAreas)
        {
            const int areaIdx = static_cast<int>(area);
            out << YAML::Key << k_DockAreaKeys[areaIdx] << YAML::Value << YAML::Flow << YAML::BeginSeq;
            for (const PanelId pid : dockLayout.m_AreaPanels[areaIdx])
                out << PanelIdToString(pid);
            out << YAML::EndSeq;
            out << YAML::Key << (std::string(k_DockAreaKeys[areaIdx]) + "_active")
                << YAML::Value << dockLayout.m_ActiveTab[areaIdx];
        }
        out << YAML::EndMap; // areas

        out << YAML::EndMap; // dock_layout
        out << YAML::EndMap; // root

        std::ofstream file(path);
        if (file.is_open())
            file << out.c_str();
    }

    bool EditorSettings::Load(const std::string& path)
    {
        if (!std::filesystem::exists(path))
            return false;

        try
        {
            YAML::Node root = YAML::LoadFile(path);

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
                if (auto n = dock["left_width"])    dockLayout.m_LeftWidth    = n.as<float>();
                if (auto n = dock["right_width"])   dockLayout.m_RightWidth   = n.as<float>();
                if (auto n = dock["bottom_height"]) dockLayout.m_BottomHeight = n.as<float>();

                if (auto areas = dock["areas"])
                {
                    constexpr DockArea k_SavedAreas[] = { DockArea::Left, DockArea::Right, DockArea::Bottom };
                    for (const DockArea area : k_SavedAreas)
                    {
                        const int areaIdx = static_cast<int>(area);
                        if (auto seq = areas[k_DockAreaKeys[areaIdx]])
                        {
                            dockLayout.m_AreaPanels[areaIdx].clear();
                            for (const auto& node : seq)
                            {
                                if (auto pid = StringToPanelId(node.as<std::string>()))
                                    dockLayout.m_AreaPanels[areaIdx].push_back(pid.value());
                            }
                        }
                        const std::string activeKey = std::string(k_DockAreaKeys[areaIdx]) + "_active";
                        if (auto n = areas[activeKey])
                            dockLayout.m_ActiveTab[areaIdx] = n.as<int>();
                    }
                }
            }
            else
            {
                dockLayout.InitDefaults();
            }

            return true;
        }
        catch (...) { return false; }
    }
}
