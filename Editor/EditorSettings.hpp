#pragma once

#include "Docking/DockTypes.hpp"
#include "Panels/ContentPanel.hpp"
#include "Project/RecentProjects.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Editor
{
    // The editor's personal state, kept per user in user://editor_settings.yaml (since HE-295),
    // never in a project.
    struct EditorSettings
    {
        // The editor's per-user folder, <LocalAppData>/HedgehogEngine/Editor, mounted by
        // EditorApplication::Init; it also holds ImGui's imgui.ini.
        static constexpr const char* USER_ALIAS = "user://";
        static constexpr const char* PATH       = "user://editor_settings.yaml";
        static constexpr const char* IMGUI_INI  = "imgui.ini";

        DockLayoutState            dockLayout;
        float                      ContentIconSize = CONTENT_ICON_SIZE_DEFAULT; // the Project grid's icon size
        bool                       PhysicsDebug    = false; // the Scene view's collider wireframes
        std::vector<RecentProject> RecentProjects; // most recent first, each with its last scene

        // Writes <file>.tmp and renames it over the file, so a cut-short write leaves the last one.
        void Save(const std::string& virtualPath, const FS::FileSystemManager& fileSystem) const;
        bool Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
    };
}
