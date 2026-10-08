#include "doctest/doctest/doctest.h"

#include "test_log_capture.hpp"

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Plugins/PluginManager.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <filesystem>
#include <string>
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
    // read, the script system registered, its plugins loaded, audio without a device and saves
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
            CHECK(Context.GetPlugins().ApplyProjectPlugins(project) == 0);
            HA::AudioEngineDesc audio;
            audio.NoDevice = true;
            REQUIRE(Context.GetAudioEngine().Init(audio));
            Context.GetSaveGames().SetSaveDirectory(saves);
        }

        EngineContext                                    Context;
        std::shared_ptr<HedgehogScripting::ScriptSystem> Scripts;
    };
}

TEST_CASE("FeatureTest - every scene loads with its project's plugins, references only existing files, plays and is "
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
            CHECK(world.Context.GetPlugins().IsLoaded("Spinner"));
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
