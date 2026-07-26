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

        // false (default): Scene + Game shown side-by-side with a draggable splitter, both always
        // rendered. true: the pre-existing tabbed layout — only the active tab's view is shown
        // (and rendered; the inactive one's pass is skipped, same as before the side-by-side UI).
        bool UseTabbedSceneGameView = false;

        void Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const;
        bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
    };
}
