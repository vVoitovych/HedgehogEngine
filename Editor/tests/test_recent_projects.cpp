#include "doctest/doctest/doctest.h"

#include "Project/RecentProjects.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Editor::RecentProject;

namespace
{
    // A temp folder holding project folders, removed at the end of the test.
    struct ProjectFolders
    {
        std::filesystem::path Root = std::filesystem::temp_directory_path() /
                                     ("recent_projects_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        ~ProjectFolders()
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }

        // A folder named name, holding a Project.yaml when withProject.
        std::filesystem::path Make(const std::string& name, bool withProject = true) const
        {
            const std::filesystem::path folder = Root / name;
            std::filesystem::create_directories(folder);
            if (withProject)
                std::ofstream(folder / "Project.yaml") << "name: " << name << "\n";
            return folder;
        }
    };

    std::vector<std::string> Names(const std::vector<RecentProject>& projects)
    {
        std::vector<std::string> names;
        for (const RecentProject& project : projects)
            names.push_back(project.Path.filename().string());
        return names;
    }
}

TEST_CASE("RecentProjects - touching a project moves it to the front and keeps its last scene")
{
    std::vector<RecentProject> projects;
    Editor::TouchRecentProject(projects, "C:/Games/A").LastScene = "assets://Scenes/A.yaml";
    Editor::TouchRecentProject(projects, "C:/Games/B").LastScene = "assets://Scenes/B.yaml";
    Editor::TouchRecentProject(projects, "C:/Games/C");
    CHECK(Names(projects) == std::vector<std::string>{ "C", "B", "A" });

    const RecentProject& a = Editor::TouchRecentProject(projects, "C:/Games/A");
    CHECK(a.LastScene == "assets://Scenes/A.yaml");
    CHECK(Names(projects) == std::vector<std::string>{ "A", "C", "B" });
    REQUIRE(Editor::FindRecentProject(projects, "C:/Games/B") != nullptr);
    CHECK(Editor::FindRecentProject(projects, "C:/Games/B")->LastScene == "assets://Scenes/B.yaml");
    CHECK(Editor::FindRecentProject(projects, "C:/Games/D") == nullptr);
}

TEST_CASE("RecentProjects - at most ten projects, the oldest dropped")
{
    std::vector<RecentProject> projects;
    for (int i = 0; i < 12; ++i)
        Editor::TouchRecentProject(projects, "C:/Games/P" + std::to_string(i));
    REQUIRE(projects.size() == Editor::MAX_RECENT_PROJECTS);
    CHECK(projects.front().Path.filename() == "P11");
    CHECK(projects.back().Path.filename() == "P2");
    CHECK(Editor::FindRecentProject(projects, "C:/Games/P1") == nullptr);
}

TEST_CASE("RecentProjects - spellings of one folder are one entry")
{
    std::vector<RecentProject> projects;
    Editor::TouchRecentProject(projects, "C:/Games/Mine").LastScene = "assets://Scenes/Level.yaml";
    for (const char* spelling : { "c:\\games\\mine", "C:/Games/Mine/", "C:\\GAMES\\Mine\\", "C:/Games/Other/../Mine" })
    {
        CAPTURE(spelling);
        CHECK(Editor::IsSameProject(spelling, "C:/Games/Mine"));
        const RecentProject& entry = Editor::TouchRecentProject(projects, spelling);
        CHECK(projects.size() == 1);
        CHECK(entry.LastScene == "assets://Scenes/Level.yaml");
    }
    CHECK_FALSE(Editor::IsSameProject("C:/Games/Mine", "C:/Games/Mine2"));
    // Stored normalized: no trailing separator.
    CHECK(projects.front().Path.has_filename());
}

TEST_CASE("RecentProjects - a relative path is stored absolute")
{
    std::vector<RecentProject> projects;
    const RecentProject& entry = Editor::TouchRecentProject(projects, "Projects/Relative");
    CHECK(entry.Path.is_absolute());
    CHECK(Editor::IsSameProject(entry.Path, std::filesystem::current_path() / "Projects" / "Relative"));
}

TEST_CASE("RecentProjects - folders without Project.yaml are removed")
{
    ProjectFolders folders;
    std::vector<RecentProject> projects;
    Editor::TouchRecentProject(projects, folders.Make("Kept"));
    Editor::TouchRecentProject(projects, folders.Make("NoProject", false));
    Editor::TouchRecentProject(projects, folders.Root / "Gone");
    Editor::TouchRecentProject(projects, folders.Make("AlsoKept"));

    CHECK(Editor::RemoveMissingProjects(projects) == 2);
    CHECK(Names(projects) == std::vector<std::string>{ "AlsoKept", "Kept" });
    CHECK(Editor::RemoveMissingProjects(projects) == 0);
}
