#pragma once

#include "Docking/DockTypes.hpp"

#include <string>

namespace Editor
{
    struct EditorSettings
    {
        float           panelBgColor[3] = { 2.0f / 255.0f, 12.0f / 255.0f, 30.0f / 255.0f };
        DockLayoutState dockLayout;

        void Save(const std::string& path) const;
        bool Load(const std::string& path);
    };
}
