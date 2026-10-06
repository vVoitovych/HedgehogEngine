#include "doctest/doctest/doctest.h"

#include "FileSystem/api/PathUtils.hpp"
#include "test_helpers.hpp"

#include <filesystem>
#include <string>

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
    // The test runs from Binaries/<Platform>/<Config>/, under the repository's Engine.yaml.
    CHECK(std::filesystem::is_regular_file(found / FS::ENGINE_ROOT_MARKER));
}

TEST_CASE("FindEngineRoot - the Engine.yaml folder from the Binaries layout, else where it started")
{
    TempDir dir;
    dir.WriteFile("Engine.yaml", "name: HedgehogEngine\n");
    const std::filesystem::path exe = dir.MakeSubdir("Binaries/windows-x86_64/Debug");
    CHECK(FS::FindEngineRoot(exe) == dir.Path());

    // A package inside the tree (Build/<name>) carries its own marker, so it is its own root.
    dir.WriteFile("Build/MyGame/Engine.yaml", "name: HedgehogEngine\n");
    const std::filesystem::path package = dir.Path() / "Build" / "MyGame";
    CHECK(FS::FindEngineRoot(package) == package);

    TempDir loose;
    const std::filesystem::path bin = loose.MakeSubdir("bin");
    loose.WriteFile("Project.yaml", "name: NotAnEngine\n"); // a project file does not mark an engine
    CHECK(FS::FindEngineRoot(bin) == bin);
}

TEST_CASE("FindAncestorHolding - the nearest folder holding a relative file, or nothing")
{
    TempDir dir;
    dir.WriteFile("Templates/Empty/Project.yaml", "name: Empty\n");
    const std::filesystem::path deep = dir.MakeSubdir("a/b/c");
    const auto found = FS::FindAncestorHolding(deep, std::filesystem::path("Templates") / "Empty" / "Project.yaml");
    REQUIRE(found.has_value());
    CHECK(*found == dir.Path());
    CHECK_FALSE(FS::FindAncestorHolding(deep, "NoSuchFile.txt").has_value());
}

TEST_CASE("FindDefaultProjectRoot - a package's own folder, else the engine's sample, else the engine root")
{
    TempDir dir;
    dir.WriteFile("Engine.yaml", "name: HedgehogEngine\n");
    const std::filesystem::path exe = dir.MakeSubdir("Binaries/windows-x86_64/Debug");
    CHECK(FS::FindDefaultProjectRoot(exe, dir.Path()) == dir.Path()); // no sample yet

    dir.WriteFile("Projects/FeatureTest/Project.yaml", "name: FeatureTest\n");
    CHECK(FS::FindDefaultProjectRoot(exe, dir.Path()) == dir.Path() / "Projects" / "FeatureTest");

    // A package beside its executable is its own project, whatever the engine has.
    dir.WriteFile("Build/MyGame/Project.yaml", "name: MyGame\n");
    CHECK(FS::FindDefaultProjectRoot(dir.Path() / "Build" / "MyGame", dir.Path()) == dir.Path() / "Build" / "MyGame");
}

TEST_CASE("GetProjectRootDirectory - the override wins until cleared; else the repository's FeatureTest")
{
    TempDir dir;
    const std::filesystem::path found  = FS::GetProjectRootDirectory();
    const std::filesystem::path engine = FS::GetEngineRootDirectory();
    // Run from Binaries/<Platform>/<Config>/: the repository is the engine and its sample project
    // is Projects/FeatureTest.
    CHECK(std::filesystem::is_regular_file(found / FS::PROJECT_FILE_NAME));
    CHECK(found == (engine / FS::DEFAULT_PROJECT_DIRECTORY).lexically_normal());

    FS::SetProjectRootDirectory(dir.Path());
    CHECK(FS::GetProjectRootDirectory() == dir.Path());
    CHECK(FS::GetEngineRootDirectory() == engine); // the engine root does not follow the project
    FS::SetProjectRootDirectory({});
    CHECK(FS::GetProjectRootDirectory() == found);
}

TEST_CASE("ToAssetVirtualPath - a path without a mount is under assets://, one naming a mount is kept")
{
    CHECK(FS::ToAssetVirtualPath("Models/crate.obj") == "assets://Models/crate.obj");
    CHECK(FS::ToAssetVirtualPath("assets://Models/crate.obj") == "assets://Models/crate.obj");
    CHECK(FS::ToAssetVirtualPath("engine://Content/Models/Default/cube.obj") == "engine://Content/Models/Default/cube.obj");
    CHECK(FS::ToAssetVirtualPath("project://Project.yaml") == "project://Project.yaml");
    CHECK(std::string(FS::ENGINE_CONTENT_PREFIX).starts_with("engine://"));
}
