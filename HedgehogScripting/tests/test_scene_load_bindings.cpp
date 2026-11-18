#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "yaml-cpp/yaml.h"

#include <memory>
#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    const std::string SCENE_A = "assets://Scenes/A.yaml";
    const std::string SCENE_B = "assets://Scenes/B.yaml";

    std::string Script(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + body +
               "\n";
    }

    // An EngineWorld whose engine reads assets:// from a temp folder too (as the prefab tests do),
    // holding Scenes/A.yaml (Plain, then Player running Scripts/A.lua) and Scenes/B.yaml (Gate
    // running Scripts/B.lua), with A open. The scripts are written by each test.
    struct SceneLoadWorld : EngineWorld
    {
        TempDir     Assets;
        ECS::Entity Player = 0;
        ECS::Entity Gate   = 0;

        SceneLoadWorld()
        {
            FS::FileSystemManager&          files   = Context.GetFileSystem();
            std::unique_ptr<FS::FileSystem> shipped = files.Unregister("assets://");
            REQUIRE(shipped);
            auto replaced = std::make_unique<FS::FileSystem>();
            for (const auto& [alias, path] : shipped->GetMountPoints())
                REQUIRE(replaced->RegisterPath(alias, alias == "assets://" ? Assets.Path() : path));
            REQUIRE(files.Register(std::move(replaced)));
            Assets.MakeSubdir("Scenes");

            HedgehogEngine::SceneManager& scenes = Context.GetSceneManager();
            scenes.ResetScene();
            Gate = AddNamed("Gate", "Scripts/B.lua");
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "B.yaml").string()));

            scenes.ResetScene();
            (void)AddNamed("Plain", "");
            Player = AddNamed("Player", "Scripts/A.lua");
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "A.yaml").string()));
            REQUIRE(scenes.LoadScene((Assets.Path() / "Scenes" / "A.yaml").string()));
            REQUIRE(scenes.GetScenePath() == SCENE_A);
        }

        ECS::Entity AddNamed(const std::string& name, const std::string& scriptPath)
        {
            const ECS::Entity entity =
                scriptPath.empty() ? Context.GetSceneManager().CreateGameObject() : AddScripted(scriptPath);
            Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
            return entity;
        }

        bool Has(const std::string& name)
        {
            for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
                if (Ecs().IsAlive(entity) && Ecs().HasComponent<ECS::HierarchyComponent>(entity) &&
                    Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name == name)
                    return true;
            return false;
        }

        // A.lua running onUpdate body; B.lua logging its start, load and SceneLoaded.
        void WriteScripts(const std::string& onUpdate)
        {
            WriteScript("A.lua", Script("A", "function A:OnUpdate(dt)\n" + onUpdate + "\nend\n"
                                             "function A:OnDestroy() print('A destroyed') end"));
            WriteScript("B.lua", Script("B", R"lua(
function B:OnStart()
    print('B started ' .. Scene.current())
    Events.subscribe('SceneLoaded', function(event) print('B heard ' .. event.path) end)
end
function B:OnLoad(state) print('B loaded') end)lua"));
        }
    };
}

TEST_CASE("Scene load bindings - a script's load from OnUpdate ends scene A's scripts and starts B's the next frame")
{
    SceneLoadWorld world;
    world.WriteScripts("if Time.frame == 2 then print('queued ' .. tostring(Scene.load('Scenes/B.yaml'))) end");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("queued true").size() == 1);
    CHECK(log.Lines("A destroyed").size() == 1);
    CHECK(log.Lines("B started").empty());
    CHECK_FALSE(world.Has("Player"));
    CHECK(world.Has("Gate"));

    world.Frame(STEP);
    CHECK(log.Lines("B started").size() == 1);
    CHECK(log.Lines("B heard " + SCENE_B).size() == 1);
    CHECK(log.Lines("B loaded").empty());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("B started").size() == 1);
    CHECK(log.Lines("B heard").size() == 1);
    CHECK(log.Lines("A destroyed").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Scene load bindings - Scene.current names A before the switch, B after it, and Stop restores A")
{
    SceneLoadWorld world;
    world.WriteScripts("if Time.frame == 2 then print('before ' .. Scene.current()) Scene.load('Scenes/B.yaml') end");
    const std::string before = world.Context.GetSceneManager().CaptureSnapshot().Yaml;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_A);
    world.Frame(STEP);
    CHECK(log.Lines("before " + SCENE_A).size() == 1);
    CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_B);
    world.Frame(STEP);
    CHECK(log.Lines("B started " + SCENE_B).size() == 1);

    REQUIRE(world.Stop());
    CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_A);
    CHECK(world.Context.GetSceneManager().CaptureSnapshot().Yaml == before);
}

TEST_CASE("Scene load bindings - paths outside assets:// and non-strings are script errors")
{
    for (const std::string call : { "Scene.load('engine://Scenes/B.yaml')", "Scene.load('Scenes/../B.yaml')",
                                    "Scene.load('C:/B.yaml')", "Scene.load(5)" })
    {
        SceneLoadWorld world;
        world.WriteScripts(call);
        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Frame(STEP);
        world.Frame(STEP);
        INFO(call);
        CHECK(log.Lines("[ERROR][Script] Player (assets://Scripts/A.lua)").size() == 1);
        CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_A);
        CHECK(log.Lines("B started").empty());
    }
}

TEST_CASE("Scene load bindings - a missing scene returns false with the engine's one error")
{
    SceneLoadWorld world;
    world.WriteScripts("if Time.frame == 1 then print('result ' .. tostring(Scene.load('Scenes/Missing.yaml'))) end");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("result false").size() == 1);
    CHECK(log.Lines("assets://Scenes/Missing.yaml does not exist").size() == 1);
    CHECK(log.Lines("[ERROR]").size() == 1);
    CHECK(world.Has("Player"));
    CHECK(log.Lines("B heard").empty());
}

TEST_CASE("Scene load bindings - a scene that is not .yaml returns false with the engine's one error")
{
    SceneLoadWorld world;
    world.Assets.WriteFile("Scenes/B.txt", "not a scene");
    world.WriteScripts("if Time.frame == 1 then print('result ' .. tostring(Scene.load('Scenes/B.txt'))) end");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("result false").size() == 1);
    CHECK(log.Lines("is not a .yaml scene").size() == 1);
    CHECK(log.Lines("[ERROR]").size() == 1);
    CHECK(world.Has("Player"));
}

TEST_CASE("Scene load bindings - a top-level call in Edit mode warns once and loads nothing")
{
    SceneLoadWorld world;
    world.WriteScript("A.lua", Script("A", "first = Scene.load('Scenes/B.yaml')\n"
                                           "second = Scene.load('Scenes/B.yaml')\n"
                                           "print('top ' .. tostring(first) .. tostring(second) .. Scene.current())"));
    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/A.lua");
    (void)world.Scripts->DescribeScript("assets://Scripts/A.lua");
    CHECK(log.Lines("Scenes load only in Play mode").size() == 1);
    CHECK(log.Lines("[Scene]").empty());
    CHECK(log.Lines("top falsefalse" + SCENE_A).size() == 2);
    CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_A);
}

TEST_CASE("Scene load bindings - a save load then a scene load in one frame starts B's script with OnStart")
{
    SceneLoadWorld world;
    TempDir        saves;
    world.Context.GetSaveGames().SetSaveDirectory(saves.Path());
    world.WriteScripts("if Time.frame == 2 then Save.write('s') end\n"
                       "if Time.frame == 4 then Save.load('s') Scene.load('Scenes/B.yaml') end");

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int i = 0; i < 5; ++i)
        world.Frame(STEP);
    CHECK(log.Lines("B started").size() == 1);
    CHECK(log.Lines("B loaded").empty());
    CHECK(log.Lines("The load of slot 's' is dropped").size() == 1);
    CHECK(world.Context.GetSceneManager().GetScenePath() == SCENE_B);
}

TEST_CASE("Scene load bindings - a scene load drops loaded script state still waiting for its entity")
{
    SceneLoadWorld world;
    // B's Gate reuses the id of A's unscripted Plain, so the entry waits in A and B's Gate would take it.
    REQUIRE(world.Gate != world.Player);
    REQUIRE(world.Ecs().IsAlive(world.Gate));
    world.WriteScripts("if Time.frame == 3 then Scene.load('Scenes/B.yaml') end");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    YAML::Node entry;
    entry["Script"]         = "assets://Scripts/B.lua";
    entry["State"]["stale"] = true;
    YAML::Node section;
    section[world.Gate] = entry; // A scripts no such entity, so the state waits
    world.Scripts->LoadScriptState(world.Ecs(), section, 1);

    world.Frame(STEP);
    world.Frame(STEP); // loads B at its end
    world.Frame(STEP);
    CHECK(log.Lines("B started").size() == 1);
    CHECK(log.Lines("B loaded").empty());
}
