#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "yaml-cpp/yaml.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + body +
               "\n";
    }

    // An EngineWorld with a temp save directory and its assets:// in a temp folder holding
    // Scenes/A.yaml (Player running Scripts/A.lua) and Scenes/B.yaml (Gate running Scripts/B.lua),
    // with A open. The scripts are written by each test.
    struct GameStateWorld : EngineWorld
    {
        TempDir     Assets;
        TempDir     Saves;
        ECS::Entity Player = 0;

        GameStateWorld()
        {
            Context.GetSaveGames().SetSaveDirectory(Saves.Path());
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
            Name(AddScripted("Scripts/B.lua"), "Gate");
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "B.yaml").string()));
            scenes.ResetScene();
            Player = AddScripted("Scripts/A.lua");
            Name(Player, "Player");
            REQUIRE(scenes.SaveScene((Assets.Path() / "Scenes" / "A.yaml").string()));
            REQUIRE(scenes.LoadScene((Assets.Path() / "Scenes" / "A.yaml").string()));
        }

        void Name(ECS::Entity entity, const std::string& name)
        {
            Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
        }

        void WriteA(const std::string& body) { WriteScript("A.lua", Script("A", body)); }

        EcsSerialization::SaveGameFile ReadSlot(const std::string& slot)
        {
            std::ifstream     in(Saves.Path() / (slot + ".save"));
            std::stringstream text;
            text << in.rdbuf();
            auto save = EcsSerialization::ReadSaveGame(text.str());
            REQUIRE(save.Value.has_value());
            return *save.Value;
        }
    };
}

TEST_CASE("Game state - a value set in scene A is read in scene B after Scene.load")
{
    GameStateWorld world;
    world.WriteA(R"lua(
function A:OnUpdate(dt)
    if Time.frame == 1 then GameState.coins = 5 GameState.inv = { sword = true } end
    if Time.frame == 2 then Scene.load('Scenes/B.yaml') end
end)lua");
    world.WriteScript("B.lua", Script("B", "function B:OnStart() print('B saw ' .. GameState.coins .. "
                                           "' sword ' .. tostring(GameState.inv.sword)) end"));
    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int i = 0; i < 4; ++i)
        world.Frame(STEP);
    CHECK(log.Lines("B saw 5 sword true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Game state - shared by every script and empty at the next Play")
{
    GameStateWorld world;
    world.WriteA("function A:OnStart() GameState.n = 1 end");
    world.WriteScript("B.lua", Script("B", "function B:OnUpdate(dt)\n"
                                           "if Time.frame == 2 then print('n ' .. tostring(GameState.n)) end end"));
    const ECS::Entity reader = world.AddScripted("Scripts/B.lua");
    LogCapture        log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("n 1").size() == 1);
    REQUIRE(world.Stop());

    world.Ecs().GetComponent<HedgehogEngine::ScriptComponent>(world.Player).Enable = false;
    REQUIRE(world.Ecs().IsAlive(reader));
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("n nil").size() == 1);
}

TEST_CASE("Game state - a top-level write in Edit is gone at Play")
{
    GameStateWorld world;
    world.WriteA("function A:OnStart() print('edit ' .. tostring(GameState.edit)) end");
    world.WriteScript("C.lua", Script("C", "GameState.edit = 1"));
    (void)world.Scripts->DescribeScript("Scripts/C.lua"); // runs C's top level in Edit
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("edit nil").size() == 1);
}

TEST_CASE("Game state - survives hot reload")
{
    using std::chrono::milliseconds;
    GameStateWorld world;
    world.WriteA("function A:OnStart() GameState.k = 3 end\nfunction A:OnUpdate(dt) end");
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    const auto t0 = std::chrono::steady_clock::now();
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0);
    world.RewriteScript("A.lua", Script("A", "function A:OnUpdate(dt) print('k ' .. GameState.k) end"));
    LogCapture log;
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0 + milliseconds(1000));
    world.Frame(STEP);
    CHECK(log.Lines("k 3").size() == 1);
}

TEST_CASE("Game state - saved and loaded with a slot")
{
    GameStateWorld world;
    world.WriteA(R"lua(
function A:OnUpdate(dt)
    if Time.frame == 1 then
        GameState.level = 2
        GameState.pos = Vector3(1, 2, 3)
        GameState.inv = { 'a', 'b', nested = { q = Quat() } }
        Save.write('s')
    end
    if Time.frame == 2 then GameState.level = 9 GameState.extra = 1 Save.load('s') end
end
function A:OnLoad(state)
    local p, q = GameState.pos, GameState.inv.nested.q
    print(string.format('level %d pos %g %g %g inv %s %s extra %s w %g', GameState.level, p.x, p.y, p.z,
                        GameState.inv[1], GameState.inv[2], tostring(GameState.extra), q.w))
end)lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int i = 0; i < 4; ++i)
        world.Frame(STEP);
    CHECK(log.Lines("level 2 pos 1 2 3 inv a b extra nil w 1").size() == 1);
    CHECK(world.ReadSlot("s").Sections.count(EcsSerialization::SAVE_SECTION_GAME_STATE) == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Game state - a function or an entity is one error naming its key, the rest saves")
{
    GameStateWorld world;
    world.WriteA(R"lua(
function A:OnUpdate(dt)
    if Time.frame == 1 then
        GameState.cb = function() end
        GameState.who = self.entity
        GameState.deep = { f = print }
        GameState.ok = 1
        Save.write('s')
    end
end)lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("GameState: not saved").size() == 3);
    CHECK(log.Lines("GameState.cb").size() == 1);
    CHECK(log.Lines("GameState.who").size() == 1);
    CHECK(log.Lines("it is an Entity").size() == 1);
    CHECK(log.Lines("GameState.deep.f").size() == 1);

    const EcsSerialization::SaveGameFile save  = world.ReadSlot("s");
    const YAML::Node                     state = save.Sections.at(EcsSerialization::SAVE_SECTION_GAME_STATE);
    CHECK(state.size() == 1);
    CHECK(state["ok"].as<int>() == 1);
    CHECK(save.Sections.at(EcsSerialization::SAVE_SECTION_SCRIPTS).size() == 1);
}

TEST_CASE("Game state - an old save without the section loads empty with no warning")
{
    GameStateWorld world;
    world.WriteA(R"lua(
function A:OnUpdate(dt)
    if Time.frame == 1 then Save.write('s') end
    if Time.frame == 3 then GameState.x = 1 Save.load('s') end
end
function A:OnLoad(state) print('x ' .. tostring(GameState.x)) end)lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);

    EcsSerialization::SaveGameFile save = world.ReadSlot("s");
    REQUIRE(save.Sections.erase(EcsSerialization::SAVE_SECTION_GAME_STATE) == 1);
    std::ofstream(world.Saves.Path() / "s.save") << EcsSerialization::WriteSaveGame(save);

    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("x nil").size() == 1);
    CHECK(log.Lines("[WARNING]").empty());
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Game state - a section that is not a map, or an entry that does not convert, warns")
{
    GameStateWorld world;
    world.WriteA("function A:OnStart() GameState.old = 1 end");
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    LogCapture log;
    world.Scripts->LoadGameState(world.Ecs(), YAML::Load("5"));
    CHECK(log.Lines("not a map").size() == 1);

    world.Scripts->LoadGameState(world.Ecs(), YAML::Load("{ bad: !vec3 [1], good: 4 }"));
    CHECK(log.Lines("GameState.bad is skipped").size() == 1);
    CHECK(log.Lines("[WARNING]").size() == 2);
    const YAML::Node saved = world.Scripts->SaveGameState(world.Ecs());
    CHECK(saved.size() == 1);
    CHECK(saved["good"].as<int>() == 4);
}

TEST_CASE("Game state - a saved entity handle is refused on load")
{
    GameStateWorld world;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    LogCapture log;
    world.Scripts->LoadGameState(world.Ecs(), YAML::Load("{ who: !entity 1, deep: { who: !entity 1 }, ok: 2 }"));
    CHECK(log.Lines("does not outlive its scene").size() == 2);
    const YAML::Node saved = world.Scripts->SaveGameState(world.Ecs());
    CHECK(saved.size() == 1);
    CHECK(saved["ok"].as<int>() == 2);
}

TEST_CASE("Game state - keys written the same way, and a table replaced through _G, are handled")
{
    GameStateWorld world;
    world.WriteA(R"lua(
function A:OnUpdate(dt)
    if Time.frame == 1 then GameState['1'] = 'text' GameState[1] = 'number' end
    if Time.frame == 2 then _G.GameState = { lost = true } end
end)lua");
    REQUIRE(world.Context.Play());
    world.Frame(STEP);

    LogCapture       log;
    const YAML::Node saved = world.Scripts->SaveGameState(world.Ecs());
    CHECK(saved.size() == 1);
    CHECK(log.Lines("another key is written the same way").size() == 1);

    world.Frame(STEP);
    REQUIRE(world.Stop());
    world.WriteA("function A:OnStart() GameState.after = 1 end\n"
                 "function A:OnUpdate(dt) print('lost ' .. tostring(GameState.lost)) end");
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("lost nil").size() == 1);
    CHECK(world.Scripts->SaveGameState(world.Ecs())["after"].as<int>() == 1);
}
