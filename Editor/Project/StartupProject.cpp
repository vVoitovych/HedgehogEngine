#include "Project/StartupProject.hpp"

#include <system_error>

namespace Editor
{
    namespace
    {
        constexpr const char* PROJECT_FILE_NAME = "Project.yaml";
    }

    bool IsProjectFolder(const std::filesystem::path& folder)
    {
        std::error_code error;
        return !folder.empty() && std::filesystem::is_regular_file(folder / PROJECT_FILE_NAME, error);
    }

    StartupProjectChoice ChooseStartupProject(const std::string& argument, const std::vector<RecentProject>& recent,
                                              const std::filesystem::path& defaultProject)
    {
        if (!argument.empty())
        {
            // A Project.yaml names its folder.
            std::filesystem::path folder = argument;
            if (folder.filename() == PROJECT_FILE_NAME)
                folder = folder.parent_path();
            if (!IsProjectFolder(NormalizeProjectPath(folder)))
                return { {}, "--project " + argument + ": " + NormalizeProjectPath(folder).string() + " holds no " +
                                 PROJECT_FILE_NAME + "." };
            return { NormalizeProjectPath(folder), {} };
        }

        for (const RecentProject& project : recent)
            if (IsProjectFolder(project.Path))
                return { NormalizeProjectPath(project.Path), {} };
        return { NormalizeProjectPath(defaultProject), {} };
    }
}
