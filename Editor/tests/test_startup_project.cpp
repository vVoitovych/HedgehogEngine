#include "doctest/doctest/doctest.h"

#include "Project/StartupProject.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Editor::RecentProject;

namespace
{
    // A temp folder holding project folders, removed at the end of the test.
    struct Folders
    {
        std::filesystem::path Root = std::filesystem::temp_directory_path() /
                                     ("startup_project_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

        ~Folders()
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }

        std::filesystem::path Make(const std::string& name, bool withProject = true) const
        {
            const std::filesystem::path folder = Root / name;
            std::filesystem::create_directories(folder);
            if (withProject)
                std::ofstream(folder / "Project.yaml") << "name: " << name << "\n";
            return folder;
        }
    };
}

TEST_CASE("StartupProject - IsProjectFolder needs a Project.yaml file")
{
    Folders folders;
    CHECK(Editor::IsProjectFolder(folders.Make("Game")));
    CHECK_FALSE(Editor::IsProjectFolder(folders.Make("Empty", false)));
    CHECK_FALSE(Editor::IsProjectFolder(folders.Root / "Gone"));
    std::filesystem::create_directories(folders.Root / "Odd" / "Project.yaml"); // a folder, not a file
    CHECK_FALSE(Editor::IsProjectFolder(folders.Root / "Odd"));
    CHECK_FALSE(Editor::IsProjectFolder({}));
}

TEST_CASE("StartupProject - --project wins over the recent list and the default")
{
    Folders folders;
    const auto named    = folders.Make("Named");
    const auto recent   = folders.Make("Recent");
    const auto fallback = folders.Make("Default");
    const std::vector<RecentProject> list = { { recent, "assets://Scenes/A.yaml" } };

    const Editor::StartupProjectChoice choice = Editor::ChooseStartupProject(named.string(), list, fallback);
    CHECK(choice.Error.empty());
    CHECK(Editor::IsSameProject(choice.Path, named));

    // Its Project.yaml names the folder too.
    const Editor::StartupProjectChoice byFile =
        Editor::ChooseStartupProject((named / "Project.yaml").string(), list, fallback);
    CHECK(byFile.Error.empty());
    CHECK(Editor::IsSameProject(byFile.Path, named));
}

TEST_CASE("StartupProject - a --project without Project.yaml is an error, whatever else exists")
{
    Folders folders;
    const auto empty    = folders.Make("NotAProject", false);
    const auto recent   = folders.Make("Recent");
    const auto fallback = folders.Make("Default");

    for (const std::filesystem::path& bad : { empty, folders.Root / "Missing", empty / "Project.yaml" })
    {
        CAPTURE(bad);
        const Editor::StartupProjectChoice choice =
            Editor::ChooseStartupProject(bad.string(), { { recent, {} } }, fallback);
        CHECK(choice.Path.empty());
        CHECK(choice.Error.find("holds no Project.yaml") != std::string::npos);
        CHECK(choice.Error.find(bad.string()) != std::string::npos);
    }
}

TEST_CASE("StartupProject - without --project, the most recent project that still exists, else the default")
{
    Folders folders;
    const auto gone     = folders.Root / "Gone";
    const auto stale    = folders.Make("Stale", false);
    const auto older    = folders.Make("Older");
    const auto oldest   = folders.Make("Oldest");
    const auto fallback = folders.Make("Default");

    const std::vector<RecentProject> list = { { gone, {} }, { stale, {} }, { older, {} }, { oldest, {} } };
    CHECK(Editor::IsSameProject(Editor::ChooseStartupProject("", list, fallback).Path, older));

    // An empty list (automated runs pass none) or only missing projects: the default.
    CHECK(Editor::IsSameProject(Editor::ChooseStartupProject("", {}, fallback).Path, fallback));
    CHECK(Editor::IsSameProject(Editor::ChooseStartupProject("", { { gone, {} }, { stale, {} } }, fallback).Path, fallback));
    CHECK(Editor::ChooseStartupProject("", {}, fallback).Error.empty());
}
