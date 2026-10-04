#include "doctest/doctest/doctest.h"

#include "CookPlan.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    constexpr const char* LEVEL = R"(Version: 1
Scene name: Level
Scene:
  - Entity: 0
    Name: Root
    Parent: 0
    Children:
      - Entity: 1
        Name: Crate
        Parent: 0
        MeshComponent:
          MeshPath: Models\crate.obj
        RenderComponent:
          Material: Materials\Crate.material
        Children: []
)";

    // A project with one scene, the files the engine always loads, and assets nothing references.
    struct FixtureProject
    {
        TempDir Project;
        TempDir Out;

        FixtureProject()
        {
            Project.WriteFile("Project.yaml", "name: Fixture\nstartup_scene: assets://Scenes/Level.yaml\n");
            Project.WriteFile("Assets/Scenes/Level.yaml", LEVEL);
            Project.WriteFile("Assets/Models/crate.obj", "o crate\n");
            Project.WriteFile("Assets/Materials/Crate.material", "Type: 0\nBaseColor: Textures/crate.png\nTransparency: 1\n");
            Project.WriteFile("Assets/Textures/crate.png", "crate");
            Project.WriteFile("Assets/Models/Default/cube.obj", "o cube\n");
            Project.WriteFile("Assets/Models/Default/sphere.obj", "o sphere\n");
            Project.WriteFile("Assets/Textures/Default/cells.png", "cells");
            // Engine graphs whose only pass draws with no shader of its own.
            for (const char* graph : { "scene", "game", "result" })
                Project.WriteFile(std::string("HedgehogEngine/HedgehogRenderer/assets/Graphs/") + graph + ".graph",
                                  "version: 2\npasses:\n  - type: Ui\n    name: Ui\n");
            // Not referenced: never packaged.
            Project.WriteFile("Assets/Textures/unused.png", "unused");
            Project.WriteFile("Assets/Scenes/Other.yaml", "Scene name: Other\nScene: []\n");
        }

        // Every file under Out, relative and with forward slashes, sorted.
        std::vector<std::string> PackagedFiles() const
        {
            std::vector<std::string> files;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(Out.Path()))
                if (entry.is_regular_file())
                    files.push_back(std::filesystem::relative(entry.path(), Out.Path()).generic_string());
            std::sort(files.begin(), files.end());
            return files;
        }
    };

    const std::vector<std::string> EXPECTED_PACKAGE = {
        "Assets/Materials/Crate.material",
        "Assets/Models/Default/cube.obj",
        "Assets/Models/Default/sphere.obj",
        "Assets/Models/crate.obj",
        "Assets/Scenes/Level.yaml",
        "Assets/Textures/Default/cells.png",
        "Assets/Textures/crate.png",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/game.graph",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/result.graph",
        "HedgehogEngine/HedgehogRenderer/assets/Graphs/scene.graph",
        "Project.yaml",
        "manifest.yaml",
    };
}

TEST_CASE("Cooker - virtual paths map onto the package layout the engine mounts")
{
    CHECK(Cooker::ToPackagePath("assets://Models/a.obj") == std::filesystem::path("Assets/Models/a.obj"));
    CHECK(Cooker::ToPackagePath("engine://Project.yaml") == std::filesystem::path("Project.yaml"));
    CHECK(Cooker::ToPackagePath("engine://HedgehogEngine/x/../y.graph") == std::filesystem::path("HedgehogEngine/y.graph"));
    CHECK_FALSE(Cooker::ToPackagePath("shaders://a.spv"));
    CHECK_FALSE(Cooker::ToPackagePath("D:/Graphs/a.graph"));
    CHECK_FALSE(Cooker::ToPackagePath("assets://../escape.png"));
    CHECK_FALSE(Cooker::ToPackagePath("engine://"));
}

TEST_CASE("Cooker - a fixture project cooks to its closure and nothing else; a second cook copies nothing")
{
    FixtureProject fixture;
    const Cooker::CookPlan plan = Cooker::BuildCookPlan(fixture.Project.Path());
    REQUIRE(plan.Errors.empty());

    const Cooker::CookResult first = Cooker::CookPackage(plan, fixture.Out.Path());
    REQUIRE(first.Errors.empty());
    CHECK(first.Copied == EXPECTED_PACKAGE.size() - 1); // the manifest is written, not copied
    CHECK(first.Unchanged == 0);
    CHECK(fixture.PackagedFiles() == EXPECTED_PACKAGE);

    const Cooker::CookResult second = Cooker::CookPackage(Cooker::BuildCookPlan(fixture.Project.Path()), fixture.Out.Path());
    REQUIRE(second.Errors.empty());
    CHECK(second.Copied == 0);
    CHECK(second.Unchanged == EXPECTED_PACKAGE.size() - 1);
    CHECK(second.Removed == 0);

    // A changed texture is copied again; a copy deleted from the package is restored.
    fixture.Project.WriteFile("Assets/Textures/crate.png", "crate, repainted");
    std::filesystem::remove(fixture.Out.Path() / "Assets/Models/crate.obj");
    const Cooker::CookResult third = Cooker::CookPackage(Cooker::BuildCookPlan(fixture.Project.Path()), fixture.Out.Path());
    CHECK(third.Copied == 2);
    CHECK(third.Unchanged == EXPECTED_PACKAGE.size() - 3);
    CHECK(fixture.PackagedFiles() == EXPECTED_PACKAGE);
}

TEST_CASE("Cooker - an asset the project stops referencing is removed from the package")
{
    FixtureProject fixture;
    REQUIRE(Cooker::CookPackage(Cooker::BuildCookPlan(fixture.Project.Path()), fixture.Out.Path()).Errors.empty());

    fixture.Project.WriteFile("Assets/Materials/Crate.material", "Type: 0\nBaseColor: \"\"\nTransparency: 1\n");
    const Cooker::CookResult result = Cooker::CookPackage(Cooker::BuildCookPlan(fixture.Project.Path()), fixture.Out.Path());
    REQUIRE(result.Errors.empty());
    CHECK(result.Removed == 1);
    CHECK(result.Copied == 1); // the material itself changed
    CHECK_FALSE(std::filesystem::exists(fixture.Out.Path() / "Assets/Textures/crate.png"));

    // An extra scene brings its closure in.
    const Cooker::CookPlan withOther = Cooker::BuildCookPlan(fixture.Project.Path(), { "assets://Scenes/Other.yaml" });
    REQUIRE(withOther.Errors.empty());
    CHECK(std::any_of(withOther.Files.begin(), withOther.Files.end(),
                      [](const Cooker::CookFile& file) { return file.VirtualPath == "assets://Scenes/Other.yaml"; }));
}

TEST_CASE("Cooker - a missing referenced asset fails the cook, naming the file that references it")
{
    FixtureProject fixture;
    fixture.Project.WriteFile("Assets/Materials/Crate.material", "Type: 0\nBaseColor: Textures/gone.png\nTransparency: 1\n");
    const Cooker::CookPlan plan = Cooker::BuildCookPlan(fixture.Project.Path());
    REQUIRE(plan.Errors.size() == 1);
    CHECK(plan.Errors[0] == "assets://Textures/gone.png (referenced by assets://Materials/Crate.material) does not exist.");

    const Cooker::CookResult result = Cooker::CookPackage(plan, fixture.Out.Path());
    CHECK(result.Errors == plan.Errors);
    CHECK(fixture.PackagedFiles().empty());

    // A required engine file is an error too, and so is a project without a startup scene.
    std::filesystem::remove(fixture.Project.Path() / "Assets/Models/Default/sphere.obj");
    fixture.Project.WriteFile("Project.yaml", "name: Fixture\n");
    const Cooker::CookPlan broken = Cooker::BuildCookPlan(fixture.Project.Path());
    CHECK(std::count(broken.Errors.begin(), broken.Errors.end(), "The project 'Fixture' has no startup scene.") == 1);
    CHECK(std::count(broken.Errors.begin(), broken.Errors.end(), "assets://Models/Default/sphere.obj does not exist.") == 1);

    std::filesystem::remove(fixture.Project.Path() / "Project.yaml");
    CHECK(Cooker::BuildCookPlan(fixture.Project.Path()).Errors.size() == 1);
}

TEST_CASE("Cooker - a shader is stale when its .spv is missing or older than its source")
{
    TempDir dir;
    const std::filesystem::path source = dir.WriteFile("Shaders/A.vert", "#version 450\nvoid main() {}\n");
    CHECK(Cooker::IsShaderStale(source));

    const std::filesystem::path spirv = dir.WriteFile("Shaders/A.vert.spv", "spirv");
    const auto                  now   = std::filesystem::last_write_time(source);
    std::filesystem::last_write_time(spirv, now + std::chrono::seconds(5));
    CHECK_FALSE(Cooker::IsShaderStale(source));
    std::filesystem::last_write_time(spirv, now - std::chrono::seconds(5));
    CHECK(Cooker::IsShaderStale(source));

    CHECK(Cooker::HashFile(source) == Cooker::HashFile(source));
    CHECK(Cooker::HashFile(source) != Cooker::HashFile(spirv));
    CHECK_FALSE(Cooker::HashFile(dir.Path() / "missing"));
}
