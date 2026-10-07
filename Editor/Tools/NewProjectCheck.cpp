#include "Tools/NewProjectCheck.hpp"

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include <system_error>

namespace Editor
{
    std::filesystem::path MakeNewProjectFolder(std::string_view name, const std::filesystem::path& parent)
    {
        return (parent / std::filesystem::path(std::string(name))).lexically_normal();
    }

    std::string CheckNewProject(std::string_view name, const std::filesystem::path& parent)
    {
        if (!HedgehogSettings::ProjectSettings::IsValidName(name))
            return "Not a project name: 1 to 64 letters, digits, '_', '-' or inner spaces.";

        std::error_code error;
        if (parent.empty() || !std::filesystem::is_directory(parent, error))
            return "The parent folder does not exist.";

        const std::filesystem::path folder = MakeNewProjectFolder(name, parent);
        if (std::filesystem::exists(folder, error) &&
            (!std::filesystem::is_directory(folder, error) || !std::filesystem::is_empty(folder, error)))
            return folder.string() + " exists and is not an empty folder.";
        return {};
    }
}
