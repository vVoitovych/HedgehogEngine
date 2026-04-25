#include "EditorSettings.hpp"

#define YAML_CPP_STATIC_DEFINE
#include "yaml-cpp/yaml.h"

#include <fstream>
#include <filesystem>

namespace Editor
{
    void EditorSettings::Save(const std::string& path) const
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "panel_bg_color" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << panelBgColor[0] << panelBgColor[1] << panelBgColor[2]
            << YAML::EndSeq;
        out << YAML::Key << "left_panel_width"     << YAML::Value << leftPanelWidth;
        out << YAML::Key << "right_panel_width"    << YAML::Value << rightPanelWidth;
        out << YAML::Key << "console_panel_height" << YAML::Value << consolePanelHeight;
        out << YAML::EndMap;

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
            if (auto n = root["left_panel_width"])     leftPanelWidth     = n.as<float>();
            if (auto n = root["right_panel_width"])    rightPanelWidth    = n.as<float>();
            if (auto n = root["console_panel_height"]) consolePanelHeight = n.as<float>();

            return true;
        }
        catch (...) { return false; }
    }
}
