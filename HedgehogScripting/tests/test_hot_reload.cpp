#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include <chrono>
#include <string>
#include <vector>

using HedgehogEngine::ScriptComponent;
using HedgehogEngine::ScriptPropertyType;

namespace
{
    using Clock = std::chrono::steady_clock;
    using std::chrono::milliseconds;

    constexpr float STEP = 1.0f / 60.0f;

    // A Counter class declaring properties (a speed by default), with the given method definitions.
    std::string Counter(const std::string& methods, const std::string& properties = "{ speed = 1.0 }")
    {
        return "Counter = setmetatable({}, { __index = ActorScript })\n"
               "Counter.__index = Counter\n"
               "Properties = " + properties + "\n" +
               "function Counter:new() return setmetatable(ActorScript:new(), Counter) end\n" + methods;
    }

    // What the test's scripts printed, in order, without the "[INFO][Script] <file>:<line>: "
    // prefix; the base ActorScript's lines and the system's own are left out.
    std::vector<std::string> Said(const LogCapture& log)
    {
        std::vector<std::string> said;
        for (const std::string& line : log.Lines("[INFO][Script]"))
        {
            if (line.find(".lua:") == std::string::npos || line.find("ActorScript.lua") != std::string::npos)
                continue;
            std::string text = line.substr(line.find(".lua:") + 5);
            text = text.substr(text.find(": ") + 2);
            text.erase(text.find_last_not_of(' ') + 1);
            said.push_back(text);
        }
        return said;
    }

    const std::string V1 = Counter(R"lua(
function Counter:OnStart() self.counter = 0 print("start") end
function Counter:OnUpdate(dt) self.counter = self.counter + 1 print("v1 " .. self.counter .. " speed " .. self.speed) end
)lua");

    const std::string V2 = Counter(R"lua(
function Counter:OnStart() print("start again") end
function Counter:OnUpdate(dt) self.counter = self.counter + 1 print("v2 " .. self.counter .. " speed " .. self.speed) end
function Counter:OnReload() print("reloaded at " .. self.counter .. " speed " .. self.speed) end
)lua");
}

TEST_CASE("Hot reload - a changed OnUpdate takes effect after the poll interval, keeping state and properties, without OnStart")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", V1);
    const ECS::Entity entity = world.AddScripted("Scripts/Counter.lua");
    HedgehogEngine::SetScriptProperty(world.Ecs().GetComponent<ScriptComponent>(entity),
                                      { "speed", ScriptPropertyType::Number, 5.0f, {} });
    const Clock::time_point t0{};

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int frame = 0; frame < 3; ++frame)
        world.Frame(STEP);
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0); // the first poll: nothing changed yet

    world.RewriteScript("Counter.lua", V2);
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0 + milliseconds(500)); // too soon: not polled
    world.Frame(STEP);
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0 + milliseconds(1000));
    world.Frame(STEP);

    const std::vector<std::string> expected = {
        "start", "v1 1 speed 5.0", "v1 2 speed 5.0", "v1 3 speed 5.0", "v1 4 speed 5.0",
        "reloaded at 4 speed 5.0", "v2 5 speed 5.0",
    };
    CHECK(Said(log) == expected);
    CHECK(log.Lines("Reloaded assets://Scripts/Counter.lua for 1 entities").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Hot reload - a syntax error keeps the old code running and logs one error with the file and line")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", V1);
    (void)world.AddScripted("Scripts/Counter.lua");
    const Clock::time_point t0{};

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.RewriteScript("Counter.lua", "Counter = (\n");
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0);
    world.Frame(STEP);
    world.Scripts->ReloadChangedScripts(world.Ecs(), t0 + milliseconds(2000)); // the same broken file: not again

    const auto errors = log.Lines("[ERROR]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("assets://Scripts/Counter.lua:") != std::string::npos);
    CHECK(Said(log) == std::vector<std::string>{ "start", "v1 1 speed 1.0", "v1 2 speed 1.0" });
    REQUIRE(world.Stop());
}

TEST_CASE("Hot reload - a property added in Edit mode appears in DescribeScript without assigning the script again")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", V1);
    (void)world.AddScripted("Scripts/Counter.lua");

    // A Play leaves the class compiled; the edit in Edit mode drops it rather than recompiling.
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    REQUIRE(world.Stop());
    CHECK(world.Scripts->DescribeScript("Scripts/Counter.lua").size() == 1);

    world.RewriteScript("Counter.lua", Counter("", "{ speed = 1.0, jump = 2.0 }"));
    const int compiled = world.Scripts->GetCompileCount();
    world.Scripts->ReloadChangedScripts(world.Ecs(), Clock::time_point{});
    CHECK(world.Scripts->GetCompileCount() == compiled);

    const auto declarations = world.Scripts->DescribeScript("Scripts/Counter.lua");
    REQUIRE(declarations.size() == 2);
    CHECK(declarations[0].Default.Name == "jump");
}

TEST_CASE("Hot reload - state copies cycles, values and handles, but not functions")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", Counter(R"lua(
function Counter:OnStart()
    self.a = { list = { 1, 2, 3 } }
    self.a.me = self.a
    self.v = Vector3(1, 2, 3)
    self.fn = function() end
    self.other = self.entity
end
)lua"));
    (void)world.AddScripted("Scripts/Counter.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.RewriteScript("Counter.lua", Counter(R"lua(
function Counter:OnReload()
    print("cycle " .. tostring(self.a.me == self.a) .. " list " .. #self.a.list .. " v " .. tostring(self.v == Vector3(1, 2, 3)))
    print("fn dropped " .. tostring(self.fn == nil) .. " handle " .. tostring(self.other == self.entity))
end
)lua"));
    world.Scripts->ReloadChangedScripts(world.Ecs(), Clock::time_point{});
    CHECK(log.Lines("cycle true list 3 v true").size() == 1);
    CHECK(log.Lines("fn dropped true handle true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Hot reload - coroutines stop, and run again only if OnReload starts them")
{
    const std::string forever = "startCoroutine(function() while true do waitFrames(1) end end)";
    EngineWorld world;
    world.WriteScript("Counter.lua", Counter("function Counter:OnStart() " + forever + " " + forever + " end\n"));
    (void)world.AddScripted("Scripts/Counter.lua");

    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Scripts->GetCoroutineCount() == 2);
    world.RewriteScript("Counter.lua", Counter("function Counter:OnReload() " + forever + " end\n"));
    world.Scripts->ReloadChangedScripts(world.Ecs(), Clock::time_point{});
    CHECK(world.Scripts->GetCoroutineCount() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Hot reload - a faulted script gets a fresh chance with the new code")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", Counter("function Counter:OnUpdate(dt) error(\"broken\") end\n"));
    (void)world.AddScripted("Scripts/Counter.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("[ERROR][Script]").size() == 1); // faulted, then skipped

    world.RewriteScript("Counter.lua", Counter("function Counter:OnUpdate(dt) print(\"fixed\") end\n"));
    world.Scripts->ReloadChangedScripts(world.Ecs(), Clock::time_point{});
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(Said(log) == std::vector<std::string>{ "fixed", "fixed" });
    REQUIRE(world.Stop());
}
