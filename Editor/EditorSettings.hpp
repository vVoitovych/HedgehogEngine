#pragma once

#include "Docking/DockTypes.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace Editor
{
    struct EditorSettings
    {
        float           panelBgColor[3] = { 2.0f / 255.0f, 12.0f / 255.0f, 30.0f / 255.0f };
        DockLayoutState dockLayout;
        std::string     LastScene;

        void Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const;
        bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
    };
}
