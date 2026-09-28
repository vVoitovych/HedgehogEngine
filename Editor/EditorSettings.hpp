#pragma once

#include "Docking/DockTypes.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace Editor
{
    struct EditorSettings
    {
        DockLayoutState dockLayout;
        std::string     LastScene;

        void Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const;
        bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
    };
}
