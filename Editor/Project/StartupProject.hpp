#pragma once

#include "Project/RecentProjects.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Which project the editor opens, chosen before the engine is built: plain data and free
// functions, no ImGui, so EditorTest compiles them directly.
namespace Editor
{
    // A folder holding Project.yaml.
    [[nodiscard]] bool IsProjectFolder(const std::filesystem::path& folder);

    struct StartupProjectChoice
    {
        std::filesystem::path Path;  // the project folder, normalized; empty on an error
        std::string           Error; // why the named project cannot open, or empty
    };

    // The project to open:
    // - argument, the --project value, when given: a project folder or its Project.yaml (an
    //   error naming it when it holds none, whatever else exists);
    // - else the first of recent (most recent first) that is still a project folder; automated
    //   runs pass none, so they stay reproducible;
    // - else defaultProject.
    [[nodiscard]] StartupProjectChoice ChooseStartupProject(const std::string&                argument,
                                                            const std::vector<RecentProject>& recent,
                                                            const std::filesystem::path&      defaultProject);
}
