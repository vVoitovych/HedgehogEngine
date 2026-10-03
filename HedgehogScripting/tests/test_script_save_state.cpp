#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "yaml-cpp/yaml.h"

#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A class named `name` with the given Properties literal and method bodies (empty ones left out).
    std::string Script(const std::string& name, const std::string& properties, const std::string& onStart,
                       const std::string& onSave = "", const std::string& onLoad = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\nProperties = " + properties + "\nfunction " + name +
                             ":new() return setmetatable(ActorScript:new(), " + name + ") end\n";
        source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onSave.empty())
            source += "function " + name + ":OnSave()\n" + onSave + "\nend\n";
        if (!onLoad.empty())
            source += "function " + name + ":OnLoad(state)\n" + onLoad + "\nend\n";
        return source;
    }

    // Changes its properties in OnStart, saves rich state, and checks all of it on load.
    const std::string PLAYER = Script("Player", "{ speed = 1.0, name = \"p\", target = EntityRef() }", R"lua(
    print("started")
    self.speed = 3.5
    self.name = "5"
    self.target = Scene.find("Friend")
)lua",
                                      R"lua(
    return {
        inventory = { "sword", "shield" },
        stats = { hp = 10, ratio = 0.25, pos = Vector3(1, 2, 3), turn = Quat(0, 0, 0, 1) },
        friend = Scene.find("Friend"),
        nested = { a = { b = { c = true } } },
        quirky = { ["5"] = "string key", [7] = "seven", empty = "", yes = "true", none = "null" },
    }
)lua",
                                      R"lua(
    assert(self.speed == 3.5 and self.name == "5" and self.target.name == "Friend", "properties")
    assert(state.inventory[1] == "sword" and state.inventory[2] == "shield" and #state.inventory == 2, "sequence")
    assert(math.type(state.stats.hp) == "integer" and state.stats.hp == 10 and state.stats.ratio == 0.25, "numbers")
    assert(state.stats.pos == Vector3(1, 2, 3) and state.stats.turn == Quat(0, 0, 0, 1), "math types")
    assert(state.friend == Scene.find("Friend") and state.friend:isValid(), "entity")
    assert(state.nested.a.b.c == true, "nesting")
    assert(state.quirky["5"] == "string key" and state.quirky[7] == "seven", "keys")
    assert(state.quirky.empty == "" and state.quirky.yes == "true" and state.quirky.none == "null", "strings")
    print("loaded ok")
)lua");

    struct SaveWorld
    {
        EngineWorld World;
        ECS::Entity Player = 0;

        SaveWorld()
        {
            World.WriteScript("Player.lua", PLAYER);
            Player = World.AddScripted("Scripts/Player.lua");
            const ECS::Entity friendEntity = World.Context.GetSceneManager().CreateGameObject();
            World.Ecs().GetComponent<ECS::HierarchyComponent>(friendEntity).Name = "Friend";
        }

        // Plays one frame and saves.
        YAML::Node PlayAndSave()
        {
            REQUIRE(World.Context.Play());
            World.Frame(STEP);
            YAML::Node section = World.Scripts->SaveScriptState(World.Ecs());
            REQUIRE(World.Stop());
            return section;
        }
    };
}

TEST_CASE("Script save state - properties and OnSave's table round-trip into running scripts")
{
    SaveWorld        world;
    LogCapture       log;
    const YAML::Node section = world.PlayAndSave();
    CHECK(log.Lines("started").size() == 1);

    // The section as text: keyed by entity id, plain data with tags for the math types and entities.
    const YAML::Node entry = section[world.Player];
    REQUIRE(entry.IsMap());
    CHECK(entry["Script"].as<std::string>() == "assets://Scripts/Player.lua");
    CHECK(entry["Properties"]["speed"].as<double>() == 3.5);
    CHECK(entry["Properties"]["target"].Tag() == "!entity");
    CHECK(entry["State"]["stats"]["pos"].Tag() == "!vec3");
    CHECK(entry["State"]["inventory"].IsSequence());

    // Through text, as a save file carries it, into a fresh Play's running scripts.
    const YAML::Node reread = YAML::Load(YAML::Dump(section));
    REQUIRE(world.World.Context.Play());
    world.World.Scripts->LoadScriptState(world.World.Ecs(), reread, 1);
    CHECK(log.Lines("loaded ok").size() == 1);
    world.World.Frame(STEP);
    CHECK(log.Lines("started").size() == 1); // a loaded script does not start again
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.World.Stop());
}

TEST_CASE("Script save state - a state loaded before Play replaces OnStart when the instance is made")
{
    SaveWorld        world;
    LogCapture       log;
    const YAML::Node section = world.PlayAndSave();

    world.World.Scripts->LoadScriptState(world.World.Ecs(), section, 1); // nothing runs yet
    CHECK(log.Lines("loaded ok").empty());
    REQUIRE(world.World.Context.Play());
    CHECK(log.Lines("loaded ok").size() == 1);
    world.World.Frame(STEP);
    CHECK(log.Lines("started").size() == 1); // only the first Play's
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.World.Stop());

    // Kept states are dropped at Stop: the next Play starts afresh.
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    CHECK(log.Lines("started").size() == 2);
    REQUIRE(world.World.Stop());
}

TEST_CASE("Script save state - a value that is not plain data leaves that script out, naming where it is")
{
    EngineWorld world;
    world.WriteScript("Good.lua", Script("Good", "{ count = 1 }", "", "    return { ok = true }"));
    world.WriteScript("Callback.lua", Script("Callback", "{}", "", "    return { handlers = { onHit = function() end } }"));
    world.WriteScript("Cycle.lua", Script("Cycle", "{}", "", "    local t = { name = \"loop\" }\n    t.again = { t }\n    return t"));
    world.WriteScript("Deep.lua", Script("Deep", "{}", "",
                                         "    local t = {}\n    local top = t\n    for i = 1, 40 do t.next = {} t = t.next end\n"
                                         "    return top"));
    world.WriteScript("Handle.lua", Script("Handle", "{}", "", "    return { where = self.entity.transform }"));
    world.WriteScript("Scalar.lua", Script("Scalar", "{}", "", "    return 5"));
    world.WriteScript("Keys.lua", Script("Keys", "{}", "", "    return { [true] = 1 }"));
    const ECS::Entity good = world.AddScripted("Scripts/Good.lua");
    for (const char* script : { "Callback", "Cycle", "Deep", "Handle", "Scalar", "Keys" })
        (void)world.AddScripted(std::string("Scripts/") + script + ".lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    const YAML::Node section = world.Scripts->SaveScriptState(world.Ecs());
    REQUIRE(world.Stop());

    CHECK(section.size() == 1);
    CHECK(section[good]["State"]["ok"].as<bool>());
    CHECK(log.Lines("Callback.lua): not saved: cannot save OnSave().handlers.onHit: it is a function").size() == 1);
    CHECK(log.Lines("Cycle.lua): not saved: cannot save OnSave().again[1]: the table contains itself").size() == 1);
    CHECK(log.Lines("Deep.lua): not saved: cannot save OnSave()" ).size() == 1);
    CHECK(log.Lines("tables nest deeper than 32").size() == 1);
    CHECK(log.Lines("Handle.lua): not saved: cannot save OnSave().where: it is a userdata that is not").size() == 1);
    CHECK(log.Lines("Scalar.lua): not saved: OnSave must return a table or nil").size() == 1);
    CHECK(log.Lines("Keys.lua): not saved: cannot save OnSave(): it has a key that is not a string or an integer").size() == 1);
}

TEST_CASE("Script save state - entries that do not fit are skipped with a warning")
{
    SaveWorld  world;
    YAML::Node section = world.PlayAndSave();
    section[world.Player]["Script"] = "assets://Scripts/Other.lua";
    section[12345]                  = "not a map";

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Scripts->LoadScriptState(world.World.Ecs(), section, 1);
    world.World.Frame(STEP);
    CHECK(log.Lines("the saved state is for assets://Scripts/Other.lua; it is not loaded").size() == 1);
    CHECK(log.Lines("The saved state of entity 12345 is skipped").size() == 1);
    CHECK(log.Lines("started").size() == 1); // not loaded, so it starts
    CHECK(log.Lines("loaded ok").empty());
    REQUIRE(world.World.Stop());
}
