#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectTemplate.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

using HedgehogSettings::ProjectSettings;

namespace
{
    // tests/ -> HedgehogSettings/ -> HedgehogEngine/ -> repository root.
    const std::filesystem::path TEMPLATE = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() /
                                           HedgehogSettings::EMPTY_TEMPLATE_DIRECTORY;

    // Every folder and file under root, relative, with forward slashes, sorted.
    std::vector<std::string> Listing(const std::filesystem::path& root, bool skipPlaceholders)
    {
        std::vector<std::string> entries;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            if (!skipPlaceholders || entry.path().filename() != HedgehogSettings::FOLDER_PLACEHOLDER_NAME)
                entries.push_back(std::filesystem::relative(entry.path(), root).generic_string());
        std::sort(entries.begin(), entries.end());
        return entries;
    }

    std::string Read(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    }

    ProjectSettings LoadProject(const std::filesystem::path& folder)
    {
        FS::FileSystemManager files;
        auto                  fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("project://", folder);
        files.Register(std::move(fs));
        ProjectSettings project;
        REQUIRE(project.Load(ProjectSettings::PATH, files));
        return project;
    }
}

TEST_CASE("Project template - the shipped Empty template is a valid project")
{
    REQUIRE(std::filesystem::is_regular_file(TEMPLATE / "Project.yaml"));
    const ProjectSettings project = LoadProject(TEMPLATE);
    CHECK(project.GetStartupScene() == "assets://Scenes/Main.yaml");
    CHECK(project.GetPlugins().empty());
    for (const char* file : { "engine_settings.yaml", "Assets/Scenes/Main.yaml", "Assets/Materials/Default.material",
                              "Assets/Input/actions.yaml" })
        CHECK(std::filesystem::is_regular_file(TEMPLATE / file));
    for (const char* folder : { "Assets/Models", "Assets/Prefabs", "Assets/Scripts", "Assets/Textures" })
        CHECK(std::filesystem::is_directory(TEMPLATE / folder));
}

TEST_CASE("Project template - CreateProject copies the template and names the project")
{
    TempDir     dir;
    const auto  target = dir.Path() / "Games" / "My Game"; // a folder that does not exist yet
    const std::string error = HedgehogSettings::CreateProject(TEMPLATE, target, "My Game");
    REQUIRE(error.empty());

    // The template's folders and files, placeholders left out; empty folders kept.
    CHECK(Listing(target, false) == Listing(TEMPLATE, true));
    CHECK(std::filesystem::is_directory(target / "Assets" / "Scripts"));
    for (const char* file : { "engine_settings.yaml", "Assets/Scenes/Main.yaml", "Assets/Materials/Default.material",
                              "Assets/Input/actions.yaml" })
    {
        CAPTURE(file);
        CHECK(Read(target / file) == Read(TEMPLATE / file));
    }

    const ProjectSettings project = LoadProject(target);
    CHECK(project.GetName() == "My Game");
    CHECK(project.GetStartupScene() == "assets://Scenes/Main.yaml");

    // An empty folder that exists is a target too.
    const auto empty = dir.MakeSubdir("Empty");
    CHECK(HedgehogSettings::CreateProject(TEMPLATE, empty, "Second").empty());
    CHECK(LoadProject(empty).GetName() == "Second");
}

TEST_CASE("Project template - a bad name, template or target fails with one reason and leaves nothing behind")
{
    TempDir dir;

    // An invalid name: nothing created.
    for (const char* bad : { "", "a/b", "..", " lead", "C:" })
    {
        CAPTURE(bad);
        const std::string error = HedgehogSettings::CreateProject(TEMPLATE, dir.Path() / "Bad", bad);
        CHECK(error.find("is not a project name") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(dir.Path() / "Bad"));
    }

    // A missing template: nothing created.
    const std::string noTemplate = HedgehogSettings::CreateProject(dir.Path() / "NoTemplate", dir.Path() / "Game", "Game");
    CHECK(noTemplate.find("is not a project template") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(dir.Path() / "Game"));

    // A folder with something in it is left as it was.
    dir.WriteFile("Taken/notes.txt", "mine");
    const std::string taken = HedgehogSettings::CreateProject(TEMPLATE, dir.Path() / "Taken", "Game");
    CHECK(taken.find("is not an empty folder") != std::string::npos);
    CHECK(Listing(dir.Path() / "Taken", false) == std::vector<std::string>{ "notes.txt" });

    // A file where the folder would go.
    dir.WriteFile("File", "x");
    CHECK(HedgehogSettings::CreateProject(TEMPLATE, dir.Path() / "File", "Game").find("is not an empty folder") !=
          std::string::npos);

    // A template whose Project.yaml does not read fails after copying: the copy is taken back, and
    // an empty target given is emptied again rather than removed.
    dir.WriteFile("Broken/Project.yaml", "name: [unclosed\n");
    dir.WriteFile("Broken/Assets/Scenes/Main.yaml", "Scene name: Main\nScene: []\n");
    const std::string broken = HedgehogSettings::CreateProject(dir.Path() / "Broken", dir.Path() / "FromBroken", "Game");
    CHECK(broken.find("cannot be read") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(dir.Path() / "FromBroken"));
    const auto givenEmpty = dir.MakeSubdir("GivenEmpty");
    CHECK_FALSE(HedgehogSettings::CreateProject(dir.Path() / "Broken", givenEmpty, "Game").empty());
    CHECK(std::filesystem::is_directory(givenEmpty));
    CHECK(std::filesystem::is_empty(givenEmpty));
}
