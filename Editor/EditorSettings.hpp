#pragma once

#include "Docking/DockTypes.hpp"
#include "Panels/ContentPanel.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace Editor
{
    struct EditorSettings
    {
        DockLayoutState dockLayout;
        std::string     LastScene;
        float           ContentIconSize = CONTENT_ICON_SIZE_DEFAULT; // the Project grid's icon size

        void Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const;
        bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
    };
}
