#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"

#include "ContentLoader/api/AssetPrefetch.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + body +
               "\n";
    }

    // A grid of quads, big enough that decoding it spans frames.
    std::string GridObj(int size)
    {
        std::string obj;
        for (int y = 0; y <= size; ++y)
            for (int x = 0; x <= size; ++x)
                obj += "v " + std::to_string(x) + " " + std::to_string(y) + " 0\n";
        obj += "vt 0 0\nvn 0 0 1\n";
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const int a = y * (size + 1) + x + 1;
                obj += "f " + std::to_string(a) + "/1/1 " + std::to_string(a + 1) + "/1/1 " +
                       std::to_string(a + size + 2) + "/1/1\n";
            }
        return obj;
    }

    // An EngineWorld whose engine reads assets:// from a temp folder holding Scenes/A.yaml (Loader
    // running Scripts/A.lua) and Scenes/B.yaml (Gate running Scripts/B.lua, and a large grid mesh),
    // with A open.
    struct AsyncSceneWorld : EngineWorld
    {
        TempDir Assets;

        AsyncSceneWorld()
        {
            ContentLoader::ClearPrefetched();
            FS::FileSystemManager&          files   = Context.GetFileSystem();
            std::unique_ptr<FS::FileSystem> shipped = files.Unregister("assets://");
            REQUIRE(shipped);
            auto replaced = std::make_unique<FS::FileSystem>();
            for (const auto& [alias, path] : shipped->GetMountPoints())
                REQUIRE(replaced->RegisterPath(alias, alias == "assets://" ? Assets.Path() : path));
            REQUIRE(files.Register(std::move(replaced)));
            Assets.MakeSubdir("Scenes");
            Assets.WriteFile("Models/grid.obj", GridObj(150));

            HedgehogEngine::SceneManager& scenes = Context.GetSceneManager();
            scenes.ResetScene();
            const ECS::Entity gate = AddScripted("Scripts/B.lua");
            Ecs().GetComponent<ECS::HierarchyComponent>(gate).Name = "Gate";
            HedgehogEngine::MeshComponent mesh;
            mesh.MeshPath = "Models/grid.obj";
            Ecs().AddComponent(gate, mesh);
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "B.yaml").string()));

            scenes.ResetScene();
            Ecs().GetComponent<ECS::HierarchyComponent>(AddScripted("Scripts/A.lua")).Name = "Loader";
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "A.yaml").string()));
            REQUIRE(scenes.LoadScene((Assets.Path() / "Scenes" / "A.yaml").string()));

            WriteScript("B.lua", Script("B", R"lua(
function B:OnStart()
    print('B started ' .. Scene.current())
    Events.subscribe('SceneLoaded', function(event) print('B heard ' .. event.path) end)
end)lua"));
        }

        ~AsyncSceneWorld() { ContentLoader::ClearPrefetched(); }
    };
}

TEST_CASE("Async scene bindings - a script's bar follows loadProgress while loading, and SceneLoaded comes once")
{
    AsyncSceneWorld world;
    world.WriteScript("A.lua", Script("A", R"lua(
function A:OnStart()
    print('queued ' .. tostring(Scene.loadAsync('Scenes/B.yaml')))
end
function A:OnUpdate(dt)
    if Scene.isLoading() then
        local width = 400 * Scene.loadProgress()
        print(string.format('loading %.4f', width))
    end
end)lua"));

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int frame = 0; frame < 5000 && log.Lines("B started").empty(); ++frame)
    {
        world.Frame(STEP);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for (int frame = 0; frame < 3; ++frame)
        world.Frame(STEP);

    CHECK(log.Lines("queued true").size() == 1);
    CHECK(log.Lines("B started assets://Scenes/B.yaml").size() == 1);
    CHECK(log.Lines("B heard assets://Scenes/B.yaml").size() == 1);

    // The bar was drawn on at least two frames of the load, never shrinking, within 0 to 400.
    const auto widths = log.Lines("loading ");
    CHECK(widths.size() >= 2);
    double last = 0.0;
    for (const std::string& line : widths)
    {
        const double width = std::atof(line.substr(line.find("loading ") + 8).c_str());
        CHECK(width >= last);
        CHECK(width <= 400.0);
        last = width;
    }
    CHECK(ContentLoader::GetPrefetchedBytes() == 0);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Async scene bindings - paths are checked as Scene.load's, and with no load the progress is 1")
{
    AsyncSceneWorld world;
    world.WriteScript("A.lua", Script("A", R"lua(
function A:OnStart()
    for _, path in ipairs({ '../B.yaml', 'engine://Scenes/B.yaml', 'C:/B.yaml' }) do
        local ok, err = pcall(function() Scene.loadAsync(path) end)
        print('bad', path, tostring(ok))
    end
    print('missing', tostring(Scene.loadAsync('Scenes/Missing.yaml')))
    print('idle', tostring(Scene.isLoading()), Scene.loadProgress())
end)lua"));
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines(" false").size() >= 3);
    CHECK(log.Lines("bad ../B.yaml false").size() == 1);
    CHECK(log.Lines("bad engine://Scenes/B.yaml false").size() == 1);
    CHECK(log.Lines("bad C:/B.yaml false").size() == 1);
    CHECK(log.Lines("missing false").size() == 1);
    CHECK(log.Lines("idle false 1.0").size() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Async scene bindings - a top-level call in Edit mode warns once and loads nothing")
{
    AsyncSceneWorld world;
    world.WriteScript("TopLevel.lua", "Scene.loadAsync('Scenes/B.yaml')\nScene.loadAsync('Scenes/B.yaml')\n"
                                      "print('described', tostring(Scene.isLoading()))\n");
    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK(log.Lines("Scene.loadAsync did nothing").size() == 1);
    CHECK(log.Lines("described false").size() == 1);
    CHECK_FALSE(world.Context.GetSceneLoadProgress().has_value());
}
