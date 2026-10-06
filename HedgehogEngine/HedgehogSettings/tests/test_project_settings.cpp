#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using HedgehogSettings::PluginEntry;
using HedgehogSettings::ProjectSettings;

namespace
{
    // engine:// on a fresh temp directory.
    struct ProjectFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        ProjectFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("engine://", Dir.Path());
            Files.Register(std::move(fs));
        }
    };

    void CheckDefaults(const ProjectSettings& project)
    {
        CHECK(project.GetName() == ProjectSettings::DEFAULT_NAME);
        CHECK(project.GetStartupScene().empty());
        CHECK(project.GetWindowTitle().empty());
        CHECK(project.GetWindowWidth() == ProjectSettings::DEFAULT_WINDOW_WIDTH);
        CHECK(project.GetWindowHeight() == ProjectSettings::DEFAULT_WINDOW_HEIGHT);
        CHECK_FALSE(project.IsFullscreen());
        CHECK(project.IsVSync());
        CHECK(project.GetGameDataVersion() == 1);
        CHECK(project.GetPlugins().empty());
    }
}

TEST_CASE("Project settings - every field round-trips through the file")
{
    ProjectFiles    files;
    ProjectSettings project;
    CHECK(project.SetName("Hedgehog Racer"));
    CHECK(project.SetStartupScene("assets://Scenes\\Levels\\Hud.yaml"));
    project.SetWindowTitle("Hedgehog Racer: Turbo");
    project.SetWindowSize(1920, 1080);
    project.SetFullscreen(true);
    project.SetVSync(false);
    project.SetGameDataVersion(3);
    CHECK(project.IsDirty());
    REQUIRE(project.Save(ProjectSettings::PATH, files.Files));
    CHECK_FALSE(project.IsDirty());

    ProjectSettings loaded;
    REQUIRE(loaded.Load(ProjectSettings::PATH, files.Files));
    CHECK_FALSE(loaded.IsDirty());
    CHECK(loaded.GetName() == "Hedgehog Racer");
    CHECK(loaded.GetStartupScene() == "assets://Scenes/Levels/Hud.yaml");
    CHECK(loaded.GetWindowTitle() == "Hedgehog Racer: Turbo");
    CHECK(loaded.GetWindowWidth() == 1920);
    CHECK(loaded.GetWindowHeight() == 1080);
    CHECK(loaded.IsFullscreen());
    CHECK_FALSE(loaded.IsVSync());
    CHECK(loaded.GetGameDataVersion() == 3);

    // Saving what was loaded writes the same text.
    const auto first = files.Files.ReadTextFile(ProjectSettings::PATH);
    REQUIRE(loaded.Save(ProjectSettings::PATH, files.Files));
    CHECK(files.Files.ReadTextFile(ProjectSettings::PATH) == first);
}

TEST_CASE("Project settings - a missing file keeps the defaults; a malformed one keeps what was there")
{
    ProjectFiles    files;
    ProjectSettings project;
    CHECK_FALSE(project.Load(ProjectSettings::PATH, files.Files));
    CheckDefaults(project);

    CHECK(project.SetName("Kept"));
    files.Dir.WriteFile("Project.yaml", "name: [unclosed\n");
    LogCapture log;
    CHECK_FALSE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(project.GetName() == "Kept");
    CHECK(log.Lines("[Project] engine://Project.yaml is malformed").size() == 1);

    // A file with only some keys gives the defaults for the rest.
    files.Dir.WriteFile("Project.yaml", "name: Partial\nwindow:\n  width: 800\n");
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(project.GetName() == "Partial");
    CHECK(project.GetWindowWidth() == 800);
    CHECK(project.GetWindowHeight() == ProjectSettings::DEFAULT_WINDOW_HEIGHT);
    CHECK(project.GetStartupScene().empty());
}

TEST_CASE("Project settings - a startup scene outside assets:// is refused")
{
    ProjectSettings project;
    CHECK(project.SetStartupScene("assets://Scenes/Default.yaml"));

    LogCapture log;
    for (const char* bad : { "engine://Assets/Scenes/Default.yaml", "C:/Scenes/Default.yaml", "Scenes/Default.yaml",
                             "assets://../Default.yaml", "assets://Scenes/../../x.yaml", "assets://Scenes/Default.txt",
                             "assets://.yaml", "assets://Scenes//Default.yaml", "assets://C:/x.yaml" })
    {
        CAPTURE(bad);
        CHECK_FALSE(ProjectSettings::IsValidStartupScene(bad));
        CHECK_FALSE(project.SetStartupScene(bad));
        CHECK(project.GetStartupScene() == "assets://Scenes/Default.yaml");
    }
    CHECK(log.Lines("is not a startup scene").size() == 9);

    // Clearing it is allowed: the game then plays its fallback.
    CHECK(project.SetStartupScene(""));
    CHECK(project.GetStartupScene().empty());

    // A file naming one is loaded with the scene left empty and a warning.
    ProjectFiles files;
    files.Dir.WriteFile("Project.yaml", "name: Game\nstartup_scene: engine://Assets/Scenes/Default.yaml\n");
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(project.GetName() == "Game");
    CHECK(project.GetStartupScene().empty());
    CHECK(log.Lines("is not a startup scene").size() == 10);
}

TEST_CASE("Project settings - names, window sizes and data versions are validated")
{
    ProjectSettings project;
    LogCapture      log;
    for (const char* bad : { "", " Leading", "Trailing ", "a/b", "a\\b", "C:", "dot.name",
                             "0123456789012345678901234567890123456789012345678901234567890123456789" })
    {
        CAPTURE(bad);
        CHECK_FALSE(ProjectSettings::IsValidName(bad));
        CHECK_FALSE(project.SetName(bad));
    }
    CHECK(project.GetName() == ProjectSettings::DEFAULT_NAME);
    CHECK_FALSE(project.IsDirty());
    CHECK(project.SetName("My_Game-2 Remastered"));

    project.SetWindowSize(1, 100000);
    CHECK(project.GetWindowWidth() == ProjectSettings::MIN_WINDOW_SIZE);
    CHECK(project.GetWindowHeight() == ProjectSettings::MAX_WINDOW_SIZE);
    project.SetGameDataVersion(-4);
    CHECK(project.GetGameDataVersion() == 1);
}

TEST_CASE("Project settings - setters mark the settings dirty only when a value changes")
{
    ProjectSettings project;
    project.SetVSync(true);
    project.SetWindowSize(ProjectSettings::DEFAULT_WINDOW_WIDTH, ProjectSettings::DEFAULT_WINDOW_HEIGHT);
    CHECK(project.SetName(ProjectSettings::DEFAULT_NAME));
    CHECK_FALSE(project.IsDirty());

    project.SetFullscreen(true);
    CHECK(project.IsDirty());
    project.CleanDirtyState();
    CHECK_FALSE(project.IsDirty());
}

TEST_CASE("Project settings - the shipped Project.yaml loads with Default.yaml as its startup scene and Spinner enabled")
{
    // tests/ -> HedgehogSettings/ -> HedgehogEngine/ -> repository root.
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("engine://", root);
    files.Register(std::move(fs));

    ProjectSettings project;
    REQUIRE(project.Load(ProjectSettings::PATH, files));
    CHECK(project.GetName() == "HedgehogEngine");
    CHECK(project.GetStartupScene() == "assets://Scenes/Default.yaml");
    CHECK(project.GetGameDataVersion() == 1);
    // The sample plugin, enabled.
    REQUIRE(project.GetPlugins().size() == 1);
    CHECK(project.GetPlugins()[0].Name == "Spinner");
    CHECK(project.GetPlugins()[0].Enabled);
    CHECK(std::filesystem::is_regular_file(root / "Assets" / "Scenes" / "Default.yaml"));
}

TEST_CASE("Project settings - plugins load in order with their flags and save to the same text")
{
    ProjectFiles files;
    files.Dir.WriteFile("Project.yaml", "name: Game\nplugins:\n  - name: Spinner\n    enabled: true\n"
                                        "  - name: Physics\n    enabled: false\n  - name: Extras\n");

    ProjectSettings project;
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    const std::vector<PluginEntry> expected = { { "Spinner", true }, { "Physics", false }, { "Extras", true } };
    CHECK(project.GetPlugins() == expected);

    REQUIRE(project.Save(ProjectSettings::PATH, files.Files));
    const auto first = files.Files.ReadTextFile(ProjectSettings::PATH);
    ProjectSettings again;
    REQUIRE(again.Load(ProjectSettings::PATH, files.Files));
    CHECK(again.GetPlugins() == expected);
    REQUIRE(again.Save(ProjectSettings::PATH, files.Files));
    CHECK(files.Files.ReadTextFile(ProjectSettings::PATH) == first);

    // No plugins is written as an empty list.
    ProjectSettings none;
    REQUIRE(none.Save(ProjectSettings::PATH, files.Files));
    CHECK(files.Files.ReadTextFile(ProjectSettings::PATH)->find("plugins: []") != std::string::npos);
}

TEST_CASE("Project settings - bad plugin names are refused by every setter and skipped by Load")
{
    ProjectSettings project;
    LogCapture      log;
    const std::vector<std::string> names = { "Two Words", "Spinner.dll", "Plugins/Spinner", std::string(65, 'a'), std::string() };
    for (const std::string& name : names)
    {
        CHECK_FALSE(ProjectSettings::IsValidPluginName(name));
        CHECK_FALSE(project.AddPlugin(name));
        CHECK_FALSE(project.SetPlugins({ PluginEntry{ "Good" }, PluginEntry{ name } }));
    }
    CHECK(project.GetPlugins().empty());
    CHECK(ProjectSettings::IsValidPluginName(std::string(64, 'a')));
    CHECK(ProjectSettings::IsValidPluginName("Hedgehog_Test-Plugin2"));

    ProjectFiles files;
    files.Dir.WriteFile("Project.yaml", "plugins:\n  - name: Good\n  - name: Bad Name\n  - just a string\n"
                                        "  - name: Flag\n    enabled: maybe\n  - name: Last\n");
    LogCapture loadLog;
    ProjectSettings loaded;
    REQUIRE(loaded.Load(ProjectSettings::PATH, files.Files));
    const std::vector<PluginEntry> expected = { { "Good", true }, { "Last", true } };
    CHECK(loaded.GetPlugins() == expected);
    CHECK(loadLog.Lines("[Project]").size() == 3);
}

TEST_CASE("Project settings - a plugin listed twice, ignoring case, is refused")
{
    ProjectSettings project;
    LogCapture      log;
    CHECK(project.AddPlugin("Spinner"));
    CHECK_FALSE(project.AddPlugin("spinner"));
    CHECK_FALSE(project.SetPlugins({ PluginEntry{ "Physics" }, PluginEntry{ "PHYSICS" } }));
    CHECK(project.GetPlugins().size() == 1);
    CHECK(log.Lines("already listed").size() == 2);
}

TEST_CASE("Project settings - plugin setters mark the settings dirty only on a change")
{
    ProjectSettings project;
    LogCapture      log;
    CHECK(project.AddPlugin("Spinner", false));
    CHECK(project.IsDirty());
    project.CleanDirtyState();

    CHECK(project.SetPluginEnabled("SPINNER", false)); // found ignoring case, already disabled
    CHECK_FALSE(project.IsDirty());
    CHECK(project.SetPluginEnabled("Spinner", true));
    CHECK(project.IsDirty());
    CHECK(project.GetPlugins()[0].Enabled);
    project.CleanDirtyState();

    CHECK(project.SetPlugins(project.GetPlugins()));
    CHECK_FALSE(project.IsDirty());
    CHECK_FALSE(project.SetPluginEnabled("Missing", true));
    CHECK_FALSE(project.RemovePlugin("Missing"));
    CHECK_FALSE(project.IsDirty());

    CHECK(project.RemovePlugin("spinner"));
    CHECK(project.IsDirty());
    CHECK(project.GetPlugins().empty());
}
