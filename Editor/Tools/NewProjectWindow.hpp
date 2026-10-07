#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Editor
{
    // File > New Project...: a project name (checked as it is typed), a parent folder (Browse...,
    // by default the open project's parent) and the folder Create makes, <parent>/<name>. Create
    // copies the engine's Templates/Empty there (HedgehogSettings::CreateProject); a failure shows
    // its reason, and Create is disabled while CheckNewProject refuses the name or folder.
    class NewProjectWindow
    {
    public:
        bool Open = false;

        // Shows the window with a fresh name, the parent of openProject as the parent folder.
        void Show(const std::filesystem::path& openProject);

        // Draws the window while open; returns the created project's folder once, which the editor
        // then switches to. editing: Create is disabled outside Edit mode.
        [[nodiscard]] std::optional<std::filesystem::path> Draw(bool editing);

    private:
        std::string m_Name;
        std::string m_Parent;
        std::string m_Status; // the last Create's failure
    };
}
