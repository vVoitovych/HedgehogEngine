#pragma once

#include <string>

namespace Editor
{
    struct EditorSettings
    {
        float panelBgColor[3]    = { 2.0f / 255.0f, 12.0f / 255.0f, 30.0f / 255.0f };
        float leftPanelWidth     = 300.0f;
        float rightPanelWidth    = 300.0f;
        float consolePanelHeight = 200.0f;

        void Save(const std::string& path) const;
        bool Load(const std::string& path);
    };
}
