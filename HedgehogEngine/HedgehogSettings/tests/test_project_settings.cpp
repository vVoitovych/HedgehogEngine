#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using HedgehogSettings::ProjectSettings;

namespace
{
    // project:// on a fresh temp directory.
    struct ProjectFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        ProjectFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("project://", Dir.Path());
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
        CHECK(project.GetMaxFps() == 0);
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
    project.SetMaxFps(144);
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
    CHECK(loaded.GetMaxFps() == 144);
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
    CHECK(log.Lines("[Project] project://Project.yaml is malformed").size() == 1);

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

TEST_CASE("Project settings - the shipped Project.yaml loads with Default.yaml as its startup scene and no plugins")
{
    // tests/ -> HedgehogSettings/ -> HedgehogEngine/ -> repository root, whose sample project is
    // Projects/FeatureTest.
    const std::filesystem::path root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path() /
                                       "Projects" / "FeatureTest";
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("project://", root);
    files.Register(std::move(fs));

    ProjectSettings project;
    REQUIRE(project.Load(ProjectSettings::PATH, files));
    CHECK(project.GetName() == "FeatureTest");
    CHECK(project.GetStartupScene() == "assets://Scenes/Default.yaml");
    CHECK(project.GetGameDataVersion() == 1);
    CHECK(std::filesystem::is_regular_file(root / "Assets" / "Scenes" / "Default.yaml"));
}

TEST_CASE("Project settings - a plugins list from an older project is ignored and not written back")
{
    ProjectFiles files;
    files.Dir.WriteFile("Project.yaml", "name: Game\ngame_data_version: 2\nplugins:\n  - name: Spinner\n    enabled: true\n");

    LogCapture      log;
    ProjectSettings project;
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(log.Lines("[Project]").empty());
    CHECK(project.GetName() == "Game");
    CHECK(project.GetGameDataVersion() == 2);

    REQUIRE(project.Save(ProjectSettings::PATH, files.Files));
    CHECK(files.Files.ReadTextFile(ProjectSettings::PATH)->find("plugins") == std::string::npos);
}

TEST_CASE("Project settings - the frame rate cap is 0 or 15 to 1000, else clamped with one warning")
{
    CHECK(ProjectSettings::ClampMaxFps(0) == 0);
    CHECK(ProjectSettings::ClampMaxFps(15) == 15);
    CHECK(ProjectSettings::ClampMaxFps(60) == 60);
    CHECK(ProjectSettings::ClampMaxFps(1000) == 1000);
    CHECK(ProjectSettings::ClampMaxFps(5) == 15);
    CHECK(ProjectSettings::ClampMaxFps(-1) == 15);
    CHECK(ProjectSettings::ClampMaxFps(5000) == 1000);

    ProjectSettings project;
    LogCapture      log;
    for (const int fine : { 0, 15, 60, 1000 })
    {
        project.SetMaxFps(fine);
        CHECK(project.GetMaxFps() == fine);
    }
    CHECK(log.Lines("[Project] window.max_fps").empty());

    project.SetMaxFps(5);
    CHECK(project.GetMaxFps() == 15);
    project.SetMaxFps(5000);
    CHECK(project.GetMaxFps() == 1000);
    project.SetMaxFps(-1);
    CHECK(project.GetMaxFps() == 15);
    CHECK(log.Lines("[Project] window.max_fps").size() == 3);

    ProjectFiles files;
    files.Dir.WriteFile("Project.yaml", "window:\n  max_fps: 5000\n");
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(project.GetMaxFps() == 1000);
    CHECK(log.Lines("[Project] window.max_fps").size() == 4);

    // A window without the key loads as no cap, silently.
    files.Dir.WriteFile("Project.yaml", "window:\n  width: 800\n");
    REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
    CHECK(project.GetMaxFps() == 0);
    CHECK(log.Lines("[Project] window.max_fps").size() == 4);

    // A value that is not a whole number is one warning; the rest of the file still reads.
    for (const char* bad : { "60.5", "fast" })
    {
        files.Dir.WriteFile("Project.yaml", std::string("window:\n  width: 800\n  max_fps: ") + bad + "\n");
        REQUIRE(project.Load(ProjectSettings::PATH, files.Files));
        CHECK(project.GetMaxFps() == 0);
        CHECK(project.GetWindowWidth() == 800);
    }
    CHECK(log.Lines("[Project] window.max_fps").size() == 6);
}

TEST_CASE("Project settings - the shipped FeatureTest and template files hold max_fps 0 and save to their text")
{
    const std::filesystem::path repo =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    for (const std::filesystem::path& root : { repo / "Projects" / "FeatureTest", repo / "Templates" / "Empty" })
    {
        CAPTURE(root.string());
        FS::FileSystemManager files;
        auto                  fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("project://", root);
        files.Register(std::move(fs));

        ProjectSettings project;
        REQUIRE(project.Load(ProjectSettings::PATH, files));
        CHECK(project.GetMaxFps() == 0);

        ProjectFiles out;
        REQUIRE(project.Save(ProjectSettings::PATH, out.Files));
        std::string shipped = *files.ReadTextFile(ProjectSettings::PATH);
        shipped.erase(std::remove(shipped.begin(), shipped.end(), '\r'), shipped.end());
        CHECK(*out.Files.ReadTextFile(ProjectSettings::PATH) == shipped);
    }
}
