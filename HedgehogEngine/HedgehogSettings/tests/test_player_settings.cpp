#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

using namespace HedgehogSettings;

namespace
{
    // player:// on a fresh temp folder.
    struct PlayerFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        PlayerFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("player://", Dir.Path());
            Files.Register(std::move(fs));
        }

        void Write(const std::string& text)
        {
            REQUIRE(Files.WriteTextFile(PLAYER_SETTINGS_PATH, text));
        }

        std::string Read() const
        {
            std::string text = Files.ReadTextFile(PLAYER_SETTINGS_PATH).value_or("");
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            return text;
        }
    };

    PlayerSettings Customized()
    {
        PlayerSettings settings;
        settings.WindowMode   = PlayerWindowMode::Borderless;
        settings.Width        = 1920;
        settings.Height       = 1080;
        settings.VSync        = false;
        settings.MaxFps       = 144;
        settings.MasterVolume = 0.5f;
        settings.BusVolumes   = { 0.25f, 0.5f, 0.75f, 0.0f };
        settings.Quality      = PlayerQuality::Low;
        return settings;
    }
}

TEST_CASE("Player settings - defaults follow the project")
{
    ProjectSettings project;
    const PlayerSettings plain = MakeDefaultPlayerSettings(project);
    CHECK(plain == PlayerSettings{});

    project.SetFullscreen(true);
    project.SetVSync(false);
    project.SetMaxFps(60);
    const PlayerSettings fullscreen = MakeDefaultPlayerSettings(project);
    CHECK(fullscreen.WindowMode == PlayerWindowMode::Fullscreen);
    CHECK_FALSE(fullscreen.VSync);
    CHECK(fullscreen.MaxFps == 60);
    CHECK(fullscreen.Width == 0);
    CHECK(fullscreen.Quality == PlayerQuality::High);
}

TEST_CASE("Player settings - every field round-trips and a second save is identical")
{
    PlayerFiles          files;
    const PlayerSettings custom = Customized();
    REQUIRE(SavePlayerSettings(files.Files, custom));
    const std::string first = files.Read();

    PlayerSettings loaded;
    REQUIRE(LoadPlayerSettings(files.Files, loaded));
    CHECK(loaded == custom);

    REQUIRE(SavePlayerSettings(files.Files, loaded));
    CHECK(files.Read() == first);
}

TEST_CASE("Player settings - a missing file is false and changes nothing")
{
    PlayerFiles    files;
    LogCapture     log;
    PlayerSettings settings = Customized();
    CHECK_FALSE(LoadPlayerSettings(files.Files, settings));
    CHECK(settings == Customized());
    CHECK(log.Text().find("[PlayerSettings]") == std::string::npos);
}

TEST_CASE("Player settings - malformed text and a non-map root are one error")
{
    for (const char* text : { "window_mode: [", "- a\n- b\n" })
    {
        CAPTURE(text);
        PlayerFiles files;
        files.Write(text);
        LogCapture     log;
        PlayerSettings settings = Customized();
        CHECK_FALSE(LoadPlayerSettings(files.Files, settings));
        CHECK(settings == Customized());
        CHECK(log.Text().find("[PlayerSettings]") != std::string::npos);
    }
}

TEST_CASE("Player settings - a bad value keeps its default with one warning while a good key reads")
{
    const char* bad[][2] = {
        { "width: -5", "width" },
        { "height: 20", "height" },
        { "window_mode: Huge", "window_mode" },
        { "quality: Epic", "quality" },
        { "master_volume: 2", "master_volume" },
        { "bus_volumes: { Sfx: .nan }", "bus_volumes.Sfx" },
        { "max_fps: 5", "max_fps" },
        { "vsync: maybe", "vsync" },
        { "bus_volumes: { Radio: 0.5 }", "'Radio'" },
        { "bus_volumes: [1, 2]", "bus_volumes" },
        { "width: 1.5", "width" },
    };
    for (const auto& [line, key] : bad)
    {
        CAPTURE(line);
        const bool     isQuality = std::string(line).starts_with("quality");
        PlayerSettings expected;
        std::string    good;
        if (isQuality)
        {
            good            = "width: 800";
            expected.Width  = 800;
        }
        else
        {
            good             = "quality: Medium";
            expected.Quality = PlayerQuality::Medium;
        }

        PlayerFiles files;
        files.Write(std::string(line) + "\n" + good + "\n");
        LogCapture     log;
        PlayerSettings settings;
        CHECK(LoadPlayerSettings(files.Files, settings));
        CHECK(settings == expected);

        const std::string text  = log.Text();
        const size_t      first = text.find("[PlayerSettings]");
        CHECK(first != std::string::npos);
        CHECK(text.find(key) != std::string::npos);
        CHECK(text.find("[PlayerSettings]", first + 1) == std::string::npos);
    }
}
