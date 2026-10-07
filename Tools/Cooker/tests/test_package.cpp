#include "doctest/doctest/doctest.h"

#include "CookPlan.hpp"
#include "Package.hpp"

#include "test_cook_fixture.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace CookTest;

namespace
{
    // The fixture project as its own engine (licences beside it) with an enabled plugin, and a
    // binaries folder holding what a build leaves there: the game's files and much it must not ship.
    struct PackageFixture : FixtureProject
    {
        TempDir Binaries;

        PackageFixture()
        {
            Project.WriteFile("Project.yaml", "name: Fixture\nstartup_scene: assets://Scenes/Level.yaml\nplugins:\n"
                                              "  - name: MyPlugin\n    enabled: true\n");
            for (const char* licence : Cooker::LICENCE_FILES)
                Project.WriteFile(licence, std::string("text of ") + licence);
            for (const char* file : Cooker::GAME_RUNTIME_FILES)
                Binaries.WriteFile(file, std::string("binary ") + file);
            Binaries.WriteFile("MyPlugin.dll", "plugin");
            for (const char* other : { "Editor.exe", "Cooker.exe", "CookerTest.exe", "Game.pdb", "Game.ilk",
                                       "HedgehogEngine.lib", "imgui.dll", "DialogueWindows.dll", "Unused.dll" })
                Binaries.WriteFile(other, "not shipped");
        }

        Cooker::PackageDesc Desc() const
        {
            return { Project.Path(), Project.Path(), Out.Path(), Binaries.Path(), {}, {} };
        }
    };

    std::vector<std::string> ExpectedPackage()
    {
        std::vector<std::string> expected = EXPECTED_PACKAGE;
        expected.insert(expected.end(), Cooker::GAME_RUNTIME_FILES.begin(), Cooker::GAME_RUNTIME_FILES.end());
        expected.insert(expected.end(), Cooker::LICENCE_FILES.begin(), Cooker::LICENCE_FILES.end());
        expected.push_back("MyPlugin.dll");
        std::sort(expected.begin(), expected.end());
        return expected;
    }
}

TEST_CASE("Package - exactly the closure, Game.exe, the runtime DLLs, the enabled plugins and the licences")
{
    PackageFixture fixture;
    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc());
    REQUIRE(result.Errors.empty());
    CHECK(fixture.PackagedFiles() == ExpectedPackage());
    CHECK(result.Files == ExpectedPackage().size() - 1); // the manifest is written, not packaged
    CHECK(result.Copied == result.Files);

    // A second package copies nothing; a plugin disabled since is removed with its DLL.
    const Cooker::PackageResult again = Cooker::PackageGame(fixture.Desc());
    REQUIRE(again.Errors.empty());
    CHECK(again.Copied == 0);
    CHECK(again.Unchanged == result.Files);

    fixture.Project.WriteFile("Project.yaml", "name: Fixture\nstartup_scene: assets://Scenes/Level.yaml\nplugins:\n"
                                              "  - name: MyPlugin\n    enabled: false\n");
    const Cooker::PackageResult disabled = Cooker::PackageGame(fixture.Desc());
    REQUIRE(disabled.Errors.empty());
    CHECK(disabled.Removed == 1);
    CHECK_FALSE(std::filesystem::exists(fixture.Out.Path() / "MyPlugin.dll"));

    // A rebuilt runtime DLL is copied again.
    fixture.Binaries.WriteFile("HedgehogEngine.dll", "rebuilt");
    CHECK(Cooker::PackageGame(fixture.Desc()).Copied == 1);
}

TEST_CASE("Package - a missing runtime file or licence fails naming it, copying nothing")
{
    PackageFixture fixture;
    std::filesystem::remove(fixture.Binaries.Path() / "HedgehogAudio.dll");
    std::filesystem::remove(fixture.Project.Path() / "THIRD_PARTY_NOTICES.md");

    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc());
    REQUIRE(result.Errors.size() == 2);
    CHECK(result.Errors[0] ==
          "The runtime file " + (fixture.Binaries.Path() / "HedgehogAudio.dll").string() + " does not exist.");
    CHECK(result.Errors[1] ==
          "The licence file " + (fixture.Project.Path() / "THIRD_PARTY_NOTICES.md").string() + " does not exist.");
    CHECK(fixture.PackagedFiles().empty());
}

TEST_CASE("Package - a forbidden file in the package fails it, naming the file")
{
    PackageFixture fixture;
    fixture.Out.WriteFile("Tools/Editor.pdb", "left over");

    const Cooker::PackageResult result = Cooker::PackageGame(fixture.Desc());
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].starts_with("The package must not contain "));
    CHECK(result.Errors[0].ends_with("Editor.pdb."));
}

TEST_CASE("Package - FindForbiddenPackageFiles finds editor, test, debug, ImGui and shader source files")
{
    TempDir out;
    for (const char* allowed : { "Game.exe", "HedgehogEngine.dll", "Assets/Scenes/a.yaml", "HedgehogEngine/x/Base.vert.spv" })
        out.WriteFile(allowed, "");
    for (const char* forbidden : { "Editor.exe", "cooker.EXE", "SpinnerTest.exe", "Game.pdb", "Game.ilk", "ECS.lib",
                                   "DialogueWindows.dll", "imgui.ini", "Shaders/Base.vert", "Shaders/Base.frag",
                                   "Shaders/Fill.comp", "Shaders/Common.glsl" })
        out.WriteFile(forbidden, "");

    const std::vector<std::filesystem::path> found = Cooker::FindForbiddenPackageFiles(out.Path());
    CHECK(found.size() == 12);
    for (const std::filesystem::path& file : found)
        CHECK(file.filename() != "Game.exe");
}

TEST_CASE("Package - AllScenes ships every scene of Assets/Scenes beside the startup scene")
{
    PackageFixture fixture;
    CHECK(Cooker::ListProjectScenes(fixture.Project.Path()) ==
          std::vector<std::string>{ "assets://Scenes/Level.yaml", "assets://Scenes/Other.yaml" });

    Cooker::PackageDesc desc = fixture.Desc();
    desc.AllScenes           = true;
    const Cooker::PackageResult result = Cooker::PackageGame(desc);
    REQUIRE(result.Errors.empty());
    std::vector<std::string> expected = ExpectedPackage();
    expected.push_back("Assets/Scenes/Other.yaml");
    std::sort(expected.begin(), expected.end());
    CHECK(fixture.PackagedFiles() == expected);

    // Without it, a scene nothing references stays out.
    CHECK(Cooker::PackageGame(fixture.Desc()).Removed == 1);
    CHECK_FALSE(std::filesystem::exists(fixture.Out.Path() / "Assets" / "Scenes" / "Other.yaml"));
}
