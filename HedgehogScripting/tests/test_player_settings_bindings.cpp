#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <string>

using HedgehogSettings::PlayerQuality;
using HedgehogSettings::PlayerSettings;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + body +
               "\n";
    }

    // A world whose player:// is a temp folder, running body as OnStart.
    struct PlayerWorld
    {
        EngineWorld World;
        TempDir     PlayerDir;

        explicit PlayerWorld(const std::string& onStart)
        {
            REQUIRE(FS::MountPlayer(World.Context.GetFileSystem(), PlayerDir.Path()));
            World.WriteScript("Menu.lua", Script("Menu", "function Menu:OnStart()\n" + onStart + "\nend"));
            (void)World.AddScripted("Scripts/Menu.lua");
        }

        void Run()
        {
            REQUIRE(World.Context.Play());
            World.Frame(STEP);
        }
    };
}

TEST_CASE("PlayerSettings bindings - writes wait until apply, and save writes what a new engine loads")
{
    PlayerWorld world("PlayerSettings.quality = 'Low'\n"
                      "PlayerSettings.setBusVolume('Music', 0.25)\n"
                      "PlayerSettings.width = 1280\n"
                      "print('pending', PlayerSettings.quality, PlayerSettings.getBusVolume('Music'),\n"
                      "      PlayerSettings.width, PlayerSettings.hasPendingChanges())\n"
                      "print('saved before apply', PlayerSettings.save())\n"
                      "PlayerSettings.apply()\n"
                      "print('applied', PlayerSettings.hasPendingChanges(), PlayerSettings.save())");
    HedgehogEngine::EngineContext& context = world.World.Context;
    const uint64_t                 version = context.GetPlayerSettingsVersion();

    LogCapture log;
    world.Run();
    CHECK(log.Lines("pending Low 0.25 1280 true").size() == 1);
    CHECK(log.Lines("saved before apply true").size() == 1);
    CHECK(log.Lines("applied false true").size() == 1);
    CHECK(context.GetPlayerSettingsVersion() == version + 1);
    CHECK(context.GetPlayerSettings().Quality == PlayerQuality::Low);
    CHECK(context.GetPlayerSettings().BusVolumes[0] == doctest::Approx(0.25f));

    // What a new engine reads from the file.
    PlayerSettings loaded;
    REQUIRE(HedgehogSettings::LoadPlayerSettings(context.GetFileSystem(), loaded));
    CHECK(loaded.Quality == PlayerQuality::Low);
    CHECK(loaded.BusVolumes[0] == doctest::Approx(0.25f));
    CHECK(loaded.Width == 1280);
}

TEST_CASE("PlayerSettings bindings - revert reads the saved values back and drops pending writes")
{
    PlayerWorld world("print('no file', PlayerSettings.revert())\n"
                      "PlayerSettings.masterVolume = 0.5\n"
                      "PlayerSettings.apply()\n"
                      "PlayerSettings.save()\n"
                      "PlayerSettings.masterVolume = 0.1\n"
                      "PlayerSettings.apply()\n"
                      "PlayerSettings.vsync = false\n"
                      "print('reverted', PlayerSettings.revert(), PlayerSettings.masterVolume,\n"
                      "      tostring(PlayerSettings.vsync), PlayerSettings.hasPendingChanges())");
    LogCapture log;
    world.Run();
    CHECK(log.Lines("no file false").size() == 1);
    CHECK(log.Lines("reverted true 0.5 true false").size() == 1);
    CHECK(world.World.Context.GetPlayerSettings().MasterVolume == doctest::Approx(0.5f));
}

TEST_CASE("PlayerSettings bindings - a bad value is a script error naming the field and its range")
{
    const std::pair<std::string, std::string> cases[] = {
        { "PlayerSettings.width = 10", "width: 10 is not a whole number from 64 to 16384" },
        { "PlayerSettings.height = 720.5", "height: 720.5 is not a whole number" },
        { "PlayerSettings.maxFps = 5", "maxFps: 5 is not a whole number from 15 to 1000, or 0 for no cap" },
        { "PlayerSettings.masterVolume = 2", "masterVolume: 2 is not a volume from 0 to 1" },
        { "PlayerSettings.masterVolume = 0/0", "masterVolume takes a finite number" },
        { "PlayerSettings.quality = 'Ultra'", "quality: 'Ultra' is not Low, Medium or High" },
        { "PlayerSettings.windowMode = 1", "windowMode: a number is not Windowed, Borderless or Fullscreen" },
        { "PlayerSettings.setBusVolume('Drums', 1)", "bus: 'Drums' is not Master, Music, Sfx, Voice or Ui" },
        { "PlayerSettings.vsync = 1", "vsync takes a boolean" },
        { "PlayerSettings.apply = 1", "PlayerSettings.apply is not a field that can be set" },
    };
    for (const auto& [statement, message] : cases)
    {
        PlayerWorld world("local ok, err = pcall(function() " + statement + " end)\n"
                          "print('ok', ok, tostring(err))\n"
                          "print('pending', PlayerSettings.hasPendingChanges())");
        LogCapture log;
        world.Run();
        INFO(statement);
        CHECK(log.Lines("ok false").size() == 1);
        CHECK(log.Lines(message).size() == 1);
        CHECK(log.Lines("pending false").size() == 1);
    }
}

TEST_CASE("PlayerSettings bindings - lists, and no resolutions without a window")
{
    PlayerWorld world("print('resolutions', #PlayerSettings.getResolutions())\n"
                      "print('levels', table.concat(PlayerSettings.getQualityLevels(), ' '))");
    LogCapture log;
    world.Run();
    CHECK(log.Lines("resolutions 0").size() == 1);
    CHECK(log.Lines("levels Low Medium High").size() == 1);

    // A host's list reaches scripts as it is.
    world.World.Stop();
    world.World.Context.GetApplicationRequests().VideoModes = { { 1280, 720, 60 }, { 1920, 1080, 144 } };
    world.World.WriteScript("Menu.lua", Script("Menu", "function Menu:OnStart()\n"
                                                       "    local r = PlayerSettings.getResolutions()[2]\n"
                                                       "    print('second', r.width, r.height, r.refreshRate)\n"
                                                       "end"));
    world.Run();
    CHECK(log.Lines("second 1920 1080 144").size() == 1);
}

TEST_CASE("PlayerSettings bindings - outside Play writes and calls warn once and change nothing")
{
    EngineWorld world;
    world.WriteScript("TopLevel.lua", "PlayerSettings.quality = 'Low'\nPlayerSettings.apply()\n"
                                      "print('described', PlayerSettings.quality, PlayerSettings.save())\n");
    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK(log.Lines("did nothing").size() == 1);
    CHECK(log.Lines("described High false").size() == 1);
    CHECK(world.Context.GetPlayerSettings().Quality == PlayerQuality::High);
}

TEST_CASE("PlayerSettings bindings - Play drops writes a previous Play left pending")
{
    PlayerWorld world("print('pending at start', PlayerSettings.hasPendingChanges())\n"
                      "PlayerSettings.quality = 'Medium'");
    LogCapture log;
    world.Run();
    REQUIRE(world.World.Stop());
    world.Run();
    CHECK(log.Lines("pending at start false").size() == 2);
    CHECK(world.World.Context.GetPlayerSettings().Quality == PlayerQuality::High);
}
