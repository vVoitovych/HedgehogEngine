#include "doctest/doctest/doctest.h"

#include "FileSystem/api/PathUtils.hpp"
#include "test_helpers.hpp"

#include <filesystem>

TEST_CASE("FindProjectRoot - the repository from the Binaries layout")
{
    TempDir dir;
    dir.WriteFile("Project.yaml", "name: Dev\n");
    const std::filesystem::path exe = dir.MakeSubdir("Binaries/windows-x86_64/Debug");
    CHECK(FS::FindProjectRoot(exe) == dir.Path());
}

TEST_CASE("FindProjectRoot - a packaged game's own folder")
{
    TempDir dir;
    dir.WriteFile("Build/MyGame/Project.yaml", "name: MyGame\n");
    const std::filesystem::path package = dir.Path() / "Build" / "MyGame";
    CHECK(FS::FindProjectRoot(package) == package);
}

TEST_CASE("FindProjectRoot - the nearest Project.yaml wins")
{
    TempDir dir;
    dir.WriteFile("Project.yaml", "name: Outer\n");
    dir.WriteFile("Games/Inner/Project.yaml", "name: Inner\n");
    const std::filesystem::path exe = dir.MakeSubdir("Games/Inner/bin");
    CHECK(FS::FindProjectRoot(exe) == dir.Path() / "Games" / "Inner");
}

TEST_CASE("FindProjectRoot - falls back to where it started")
{
    TempDir dir;
    const std::filesystem::path exe = dir.MakeSubdir("Loose/bin");
    // A folder named Project.yaml does not count as the file.
    dir.MakeSubdir("Loose/Project.yaml");
    CHECK(FS::FindProjectRoot(exe) == exe);
}

TEST_CASE("GetEngineRootDirectory - the override wins until it is cleared")
{
    TempDir dir;
    const std::filesystem::path found = FS::GetEngineRootDirectory();
    FS::SetEngineRootDirectory(dir.Path());
    CHECK(FS::GetEngineRootDirectory() == dir.Path());
    FS::SetEngineRootDirectory({});
    CHECK(FS::GetEngineRootDirectory() == found);
    // The test runs from Binaries/<Platform>/<Config>/, under the repository's Project.yaml.
    CHECK(std::filesystem::is_regular_file(found / FS::PROJECT_FILE_NAME));
}
