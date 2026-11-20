#include "doctest/doctest/doctest.h"

#include "test_log_capture.hpp"

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/App/ApplicationRequests.hpp"
#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

using HedgehogEngine::EngineContext;

namespace
{
    constexpr float STEP   = 1.0f / 60.0f;
    constexpr int   FRAMES = 120;

    // The sample project's scenes (the dev tree's default project, Projects/FeatureTest), by
    // name: a scene added later is covered with no change here.
    std::vector<std::string> ListScenes()
    {
        EngineContext            context;
        std::vector<std::string> scenes;
        if (const auto entries = context.GetFileSystem().ListDirectory("assets://Scenes"))
            for (const FS::DirectoryEntry& entry : *entries)
                if (!entry.IsDirectory && entry.Name.ends_with(".yaml"))
                    scenes.push_back("assets://Scenes/" + entry.Name);
        return scenes;
    }

    // The engine as the Editor and game mode set it up for the project: its settings and project
    // read, the script system registered, audio without a device and saves
    // into a temp folder.
    struct ProjectWorld
    {
        explicit ProjectWorld(const std::filesystem::path& saves)
        {
            auto& files = Context.GetFileSystem();
            REQUIRE(Context.GetSettings().Load(HedgehogSettings::Settings::PATH, files));
            auto& project = Context.GetSettings().GetProjectSettings();
            REQUIRE(project.Load(HedgehogSettings::ProjectSettings::PATH, files));
            Scripts = HedgehogScripting::RegisterScriptSystem(Context, files);
            HA::AudioEngineDesc audio;
            audio.NoDevice = true;
            REQUIRE(Context.GetAudioEngine().Init(audio));
            Context.GetSaveGames().SetSaveDirectory(saves);
        }

        EngineContext                                    Context;
        std::shared_ptr<HedgehogScripting::ScriptSystem> Scripts;
    };
}

TEST_CASE("FeatureTest - every scene loads with its project, references only existing files, plays and is "
          "restored by Stop")
{
    const std::vector<std::string> scenes = ListScenes();
    REQUIRE(scenes.size() >= 9);

    for (const std::string& scene : scenes)
    {
        CAPTURE(scene);
        TempDir    saves;
        LogCapture log;
        {
            ProjectWorld world(saves.Path());
            const FS::FileSystemManager& files = world.Context.GetFileSystem();

            // Every file the scene reaches exists.
            EcsSerialization::AssetDependencyCollector collector;
            HedgehogEngine::RegisterEngineAssetDependencies(collector);
            const EcsSerialization::AssetDependencies dependencies = collector.CollectScene(scene, files);
            CHECK(dependencies.Warnings.empty());
            for (const std::string& warning : dependencies.Warnings)
                MESSAGE(warning);

            const auto path = files.ResolvePhysical(scene);
            REQUIRE(path.has_value());
            REQUIRE(world.Context.GetSceneManager().LoadScene(path->string()));
            const HedgehogEngine::SceneSnapshot before = world.Context.GetSceneManager().CaptureSnapshot();

            // Two seconds of Play, as the Editor runs a frame, then Stop puts the scene back.
            REQUIRE(world.Context.Play());
            for (int frame = 0; frame < FRAMES; ++frame)
                world.Context.UpdateContext(1.0f, STEP);
            REQUIRE(world.Context.Stop());
            CHECK(world.Context.GetSceneManager().CaptureSnapshot().Yaml == before.Yaml);
        }

        CHECK(log.Lines("[ERROR]").empty());
        CHECK(log.Lines("[WARNING]").empty());
        for (const std::string& line : log.Lines("[ERROR]"))
            MESSAGE(line);
        for (const std::string& line : log.Lines("[WARNING]"))
            MESSAGE(line);

        // The save sample wrote its slot on frame 30 and, loading it on frame 60, found its entity
        // back where it was saved.
        if (scene.ends_with("/Saves.yaml"))
        {
            CHECK(std::filesystem::is_regular_file(saves.Path() / "SaveDemo.save"));
            CHECK(log.Lines("SaveDemo: saving slot SaveDemo at frame 30: true").size() == 1);
            CHECK(log.Lines("SaveDemo: loading slot SaveDemo at frame 60: true").size() == 1);
            CHECK(log.Lines("SaveDemo: loaded; the entity is back at").size() == 1);
        }

        // The audio sample streams its music and the mix demo halves the Music bus, then puts it back.
        if (scene.ends_with("/Audio.yaml"))
        {
            CHECK(log.Lines("AudioMixDemo: Music bus at 0.5").size() == 1);
            CHECK(log.Lines("AudioMixDemo: Music bus back at 1").size() == 1);
        }

        // The physics sample: the ray found the stack, the ball fell through the trigger zone, the
        // first thing it landed on was the floor, and the kinematic paddle pushed the crate.
        if (scene.ends_with("/Physics.yaml"))
        {
            CHECK(log.Lines("PhysicsDemo: the ray down hit StackTop").size() == 1);
            CHECK(log.Lines("PhysicsDemo: Ball entered the zone Zone").size() == 1);
            CHECK(log.Lines("PhysicsDemo: ball landed on Floor").size() == 1);
            CHECK(log.Lines("Paddle: pushed Crate").size() == 1);
        }
    }
}

namespace
{
    constexpr float PLAY_X = 960.0f;
    constexpr float PLAY_Y = 540.0f;
    constexpr float OPTIONS_Y = 680.0f;
    constexpr float QUIT_Y    = 820.0f;

    void Step(ProjectWorld& world, const HW::RawInput& input)
    {
        world.Context.UpdateGameInput(input, HM::Vector2(1920.0f, 1080.0f));
        world.Context.UpdateContext(1.0f, STEP);
    }

    HW::RawInput Pointer(float x, float y, bool down = false)
    {
        HW::RawInput input;
        input.CursorPosition = HM::Vector2(x, y);
        input.CursorInside   = true;
        input.CursorKnown    = true;

        input.MouseButtons[static_cast<size_t>(HW::MouseButton::Left)] = down;
        return input;
    }

    // A click loads the scene at the end of its frame and the new scripts start at the next sync, so two more
    // frames pass before their OnStart has run.
    void Click(ProjectWorld& world, float x, float y)
    {
        Step(world, Pointer(x, y));
        Step(world, Pointer(x, y, true));
        Step(world, Pointer(x, y));
        Step(world, HW::RawInput{});
        Step(world, HW::RawInput{});
    }

    bool InScene(ProjectWorld& world, std::string_view name)
    {
        return world.Context.GetSceneManager().GetScenePath().ends_with(name);
    }
}

TEST_CASE("FeatureTest - the game flow menu loads the level, Escape returns, and quit asks to end the game")
{
    TempDir    saves;
    LogCapture log;
    {
        ProjectWorld world(saves.Path());
        auto&        files = world.Context.GetFileSystem();
        const auto   path  = files.ResolvePhysical("assets://Scenes/GameFlowMenu.yaml");
        REQUIRE(path.has_value());
        REQUIRE(world.Context.GetSceneManager().LoadScene(path->string()));
        const HedgehogEngine::SceneSnapshot before = world.Context.GetSceneManager().CaptureSnapshot();
        REQUIRE(world.Context.Play());
        Step(world, HW::RawInput{});

        Click(world, PLAY_X, PLAY_Y);
        CHECK(InScene(world, "Scenes/GameFlowLevel.yaml"));
        CHECK(log.Lines("GameFlowLevel: visit 1").size() == 1);
        CHECK(world.Context.GetApplicationRequests().Cursor == HW::CursorMode::Locked);

        HW::RawInput escape;
        escape.Keys[static_cast<size_t>(HW::Key::Escape)] = true;
        Step(world, escape);
        Step(world, HW::RawInput{});
        Step(world, HW::RawInput{});
        CHECK(InScene(world, "Scenes/GameFlowMenu.yaml"));
        CHECK(world.Context.GetApplicationRequests().Cursor == HW::CursorMode::Normal);

        // GameState survived both loads.
        Click(world, PLAY_X, PLAY_Y);
        CHECK(log.Lines("GameFlowLevel: visit 2").size() == 1);

        Step(world, escape);
        Step(world, HW::RawInput{});
        Step(world, HW::RawInput{});
        REQUIRE(InScene(world, "Scenes/GameFlowMenu.yaml"));
        CHECK_FALSE(world.Context.GetApplicationRequests().QuitRequested);
        Click(world, PLAY_X, QUIT_Y);
        CHECK(world.Context.GetApplicationRequests().QuitRequested);

        REQUIRE(world.Context.Stop());
        CHECK(world.Context.GetSceneManager().CaptureSnapshot().Yaml == before.Yaml);
    }

    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("[WARNING]").empty());
}

namespace
{
    // Options.yaml's value buttons are a column 250 px right of centre, a row every 80 px from 330 px up.
    constexpr float VALUE_X = 1210.0f;

    float RowY(int row) { return 540.0f - 330.0f + 80.0f * static_cast<float>(row); }

    HW::RawInput Key(HW::Key key)
    {
        HW::RawInput input;
        input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }
}

TEST_CASE("FeatureTest - the options menu applies and saves its choices and a rebound key, and Escape returns")
{
    TempDir    saves;
    TempDir    player;
    LogCapture log;
    {
        ProjectWorld world(saves.Path());
        auto&        files = world.Context.GetFileSystem();
        REQUIRE(FS::MountPlayer(files, player.Path()));
        const auto path = files.ResolvePhysical("assets://Scenes/GameFlowMenu.yaml");
        REQUIRE(path.has_value());
        REQUIRE(world.Context.GetSceneManager().LoadScene(path->string()));
        REQUIRE(world.Context.Play());
        Step(world, HW::RawInput{});
        Click(world, PLAY_X, OPTIONS_Y);
        REQUIRE(InScene(world, "Scenes/Options.yaml"));

        Click(world, VALUE_X, RowY(6)); // quality: High to Low
        Click(world, VALUE_X, RowY(4)); // master volume: 100% to 0%
        Click(world, VALUE_X, RowY(7)); // the Submit key: listen
        Step(world, Key(HW::Key::F));
        Step(world, HW::RawInput{});
        Click(world, 560.0f, 880.0f);
        Click(world, PLAY_X, 880.0f);
        CHECK(log.Lines("OptionsMenu: Applied").size() == 1);
        CHECK(log.Lines("OptionsMenu: Saved").size() == 1);

        HedgehogSettings::PlayerSettings saved;
        REQUIRE(HedgehogSettings::LoadPlayerSettings(files, saved));
        CHECK(saved.Quality == HedgehogSettings::PlayerQuality::Low);
        CHECK(saved.MasterVolume == doctest::Approx(0.0f));
        CHECK(world.Context.GetPlayerSettings().Quality == HedgehogSettings::PlayerQuality::Low);
        REQUIRE(world.Context.LoadInputOverrides());
        REQUIRE(world.Context.GetInputOverrides().Actions.size() == 1);
        CHECK(world.Context.GetInputOverrides().Actions[0].Bindings[0].Code == static_cast<uint16_t>(HW::Key::F));

        Step(world, Key(HW::Key::Escape));
        Step(world, HW::RawInput{});
        Step(world, HW::RawInput{});
        CHECK(InScene(world, "Scenes/GameFlowMenu.yaml"));
        REQUIRE(world.Context.Stop());
    }

    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("[WARNING]").empty());
}
