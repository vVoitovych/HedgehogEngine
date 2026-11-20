#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ContentLoader/api/AssetPrefetch.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Text += line + '\n'; }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        [[nodiscard]] size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = m_Text.find(fragment); at != std::string::npos; at = m_Text.find(fragment, at + 1))
                ++count;
            return count;
        }

    private:
        std::string m_Text;
        int         m_Sink = 0;
    };

    // A grid of quads, big enough that decoding it takes a moment.
    std::string GridObj(int size)
    {
        std::string obj;
        for (int y = 0; y <= size; ++y)
            for (int x = 0; x <= size; ++x)
                obj += "v " + std::to_string(x) + " " + std::to_string(y) + " 0\n";
        obj += "vt 0 0\nvn 0 0 1\n";
        const int row = size + 1;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const int a = y * row + x + 1;
                obj += "f " + std::to_string(a) + "/1/1 " + std::to_string(a + 1) + "/1/1 " +
                       std::to_string(a + row + 1) + "/1/1\n";
            }
        return obj;
    }

    // An engine with scenes:// on a temp folder holding A (one object) and B (two meshes, one of
    // them a large grid), A loaded.
    struct AsyncWorld
    {
        TempDir                  Dir;
        EngineContext            Context;
        SceneManager&            Scenes = Context.GetSceneManager();
        std::vector<std::string> Loaded;

        AsyncWorld()
        {
            ContentLoader::ClearPrefetched();
            Mount(Context, Dir);
            Context.GetEventBus().Subscribe<SceneLoadedEvent>([this](const SceneLoadedEvent& event)
                                                              { Loaded.push_back(event.Path); });
            Dir.WriteFile("Models/grid.obj", GridObj(120));
            Dir.WriteFile("Models/tri.obj", "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvn 0 0 1\nf 1/1/1 2/1/1 3/1/1\n");
            Dir.WriteFile("Broken.yaml", "Scene: [unclosed\n");

            Scenes.ResetScene();
            Scenes.SetSceneName("A");
            (void)Scenes.CreateGameObject();
            REQUIRE(Scenes.SaveScene((Dir.Path() / "A.yaml").string()));

            Scenes.ResetScene();
            Scenes.SetSceneName("B");
            for (const char* mesh : { "scenes://Models/grid.obj", "scenes://Models/tri.obj" })
            {
                const ECS::Entity entity = Scenes.CreateGameObject();
                MeshComponent     component;
                component.MeshPath = mesh;
                Context.GetECS().AddComponent(entity, component);
            }
            REQUIRE(Scenes.SaveScene((Dir.Path() / "B.yaml").string()));
            REQUIRE(Scenes.LoadScene((Dir.Path() / "A.yaml").string()));
        }

        ~AsyncWorld() { ContentLoader::ClearPrefetched(); }

        static void Mount(EngineContext& context, const TempDir& dir)
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("scenes://", dir.Path());
            REQUIRE(context.GetFileSystem().Register(std::move(fs)));
        }

        void Frame() { Context.UpdateContext(1.0f, FRAME); }

        // Frames until the background load has swapped in, checking its progress never falls.
        int FramesUntilLoaded()
        {
            float last = 0.0f;
            for (int frame = 1; frame <= 5000; ++frame)
            {
                Frame();
                const std::optional<float> progress = Context.GetSceneLoadProgress();
                if (!progress)
                    return frame;
                CHECK(*progress >= last);
                CHECK(*progress <= 1.0f);
                last = *progress;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return -1;
        }
    };
}

TEST_CASE("Async scene load - the current scene plays on, then the swap equals a synchronous load")
{
    AsyncWorld world;
    REQUIRE(world.Context.Play());
    LogCapture log;
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml", true));
    REQUIRE(world.Context.GetSceneLoadProgress().has_value());

    world.Frame();
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml"); // still A, and the frame ran

    const int frames = world.FramesUntilLoaded();
    REQUIRE(frames > 0);
    CHECK(world.Scenes.GetScenePath() == "scenes://B.yaml");
    CHECK(world.Loaded == std::vector<std::string>{ "scenes://B.yaml" });
    CHECK(log.Count("[Scene] Loaded scenes://B.yaml in the background in") == 1);
    CHECK(log.Count("could not be loaded") == 0);

    // The meshes the swap listed load from the store in that frame's Sync phase; the next frame
    // drops whatever no load took.
    world.Frame();
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);

    // The same state as LoadScene of the same file.
    const std::string asyncYaml = world.Scenes.CaptureSnapshot().Yaml;
    EngineContext     direct;
    AsyncWorld::Mount(direct, world.Dir);
    REQUIRE(direct.GetSceneManager().LoadScene((world.Dir.Path() / "B.yaml").string()));
    CHECK(direct.GetSceneManager().CaptureSnapshot().Yaml == asyncYaml);

    REQUIRE(world.Context.Stop());
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");
}

TEST_CASE("Async scene load - Stop or a later request mid-load leaves no job and no prefetched bytes")
{
    AsyncWorld world;
    REQUIRE(world.Context.Play());
    const std::string yamlA = world.Scenes.CaptureSnapshot().Yaml;

    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml", true));
    REQUIRE(world.Context.Stop());
    CHECK_FALSE(world.Context.GetSceneLoadProgress().has_value());
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    CHECK(world.Scenes.CaptureSnapshot().Yaml == yamlA);
    CHECK(world.Loaded.empty());

    // A synchronous request cancels the background one; its scene is the one that loads.
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml", true));
    REQUIRE(world.Context.RequestSceneLoad("scenes://A.yaml"));
    CHECK_FALSE(world.Context.GetSceneLoadProgress().has_value());
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    for (int i = 0; i < 5; ++i)
        world.Frame();
    CHECK(world.Loaded == std::vector<std::string>{ "scenes://A.yaml" });

    // A second background request replaces the first.
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml", true));
    REQUIRE(world.Context.RequestSceneLoad("scenes://A.yaml", true));
    REQUIRE(world.FramesUntilLoaded() > 0);
    CHECK(world.Loaded == std::vector<std::string>{ "scenes://A.yaml", "scenes://A.yaml" });
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Async scene load - a scene that does not parse fails with one error and keeps the current scene")
{
    AsyncWorld world;
    REQUIRE(world.Context.Play());
    const std::string yamlA = world.Scenes.CaptureSnapshot().Yaml;
    LogCapture        log;
    CHECK_FALSE(world.Context.RequestSceneLoad("scenes://Missing.yaml", true));
    REQUIRE(world.Context.RequestSceneLoad("scenes://Broken.yaml", true));
    REQUIRE(world.FramesUntilLoaded() > 0);
    CHECK(log.Count("[Scene] scenes://Missing.yaml does not exist.") == 1);
    CHECK(log.Count("[Scene] scenes://Broken.yaml could not be loaded (it does not parse") == 1);
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");
    CHECK(world.Scenes.CaptureSnapshot().Yaml == yamlA);
    CHECK(world.Loaded.empty());
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    REQUIRE(world.Context.Stop());
}
