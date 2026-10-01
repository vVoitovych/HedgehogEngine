#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given OnStart and OnUpdate bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    // What the test's scripts printed, in order, without the "[INFO][Script] <file>:<line>: "
    // prefix; the base ActorScript's own lines are left out.
    std::vector<std::string> Said(const LogCapture& log)
    {
        std::vector<std::string> said;
        for (const std::string& line : log.Lines("[INFO][Script]"))
        {
            if (line.find("ActorScript.lua") != std::string::npos)
                continue;
            std::string text = line.substr(line.find(": ") + 2);
            text.erase(text.find_last_not_of(' ') + 1);
            said.push_back(text);
        }
        return said;
    }

    // Subscribes to "hit" in OnStart and prints what arrives.
    const std::string LISTENER = R"lua(
    Events.subscribe("hit", function(payload, name)
        print("got " .. name .. " " .. payload.damage .. " " .. payload.tags[2] .. " " .. tostring(payload.source == shooter))
    end)
)lua";
}

TEST_CASE("Script events - a publish in OnUpdate reaches a handler the same frame, after every OnUpdate")
{
    EngineWorld world;
    world.WriteScript("Listener.lua", Script("Listener", LISTENER, "    print(\"listener update\")"));
    world.WriteScript("Shooter.lua", Script("Shooter", "", R"lua(
    print("shooter update")
    if not fired then
        fired = true
        shooter = self.entity
        Events.publish("hit", { damage = 7, tags = { "fire", "crit" }, source = self.entity })
    end
)lua"));
    // The listener runs first, so a same-frame delivery can only come after the shooter's OnUpdate.
    (void)world.AddScripted("Scripts/Listener.lua");
    (void)world.AddScripted("Scripts/Shooter.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    const std::vector<std::string> expected = { "listener update", "shooter update", "got hit 7 crit false" };
    CHECK(Said(log) == expected);
    CHECK(world.Scripts->GetSubscriptionCount() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Script events - the handler runs with its owner's globals, and receives entity handles intact")
{
    EngineWorld world;
    world.WriteScript("Owner.lua", Script("Owner", R"lua(
    mine = "owner's"
    Events.subscribe("ping", function(payload) print("as " .. mine .. " from " .. payload.from.name) end)
)lua"));
    world.WriteScript("Pinger.lua", Script("Pinger", "    mine = \"pinger's\"\n    self.entity.name = \"Pinger\"",
                                           "    if not done then done = true Events.publish(\"ping\", { from = self.entity }) end"));
    (void)world.AddScripted("Scripts/Owner.lua");
    (void)world.AddScripted("Scripts/Pinger.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("as owner's from Pinger").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Script events - unsubscribe stops delivery")
{
    EngineWorld world;
    world.WriteScript("Once.lua", Script("Once", R"lua(
    id = Events.subscribe("tick", function()
        print("tick")
        print("unsubscribed " .. tostring(Events.unsubscribe(id)) .. " again " .. tostring(Events.unsubscribe(id)))
    end)
)lua",
                                         "    Events.publish(\"tick\")"));
    (void)world.AddScripted("Scripts/Once.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("tick").size() == 1);
    CHECK(log.Lines("unsubscribed true again false").size() == 1);
    CHECK(world.Scripts->GetSubscriptionCount() == 0);
    REQUIRE(world.Stop());
}

TEST_CASE("Script events - a destroyed subscriber receives nothing and its subscription is released")
{
    EngineWorld world;
    world.WriteScript("Listener.lua", Script("Listener", LISTENER));
    world.WriteScript("Shooter.lua", Script("Shooter", "",
                                            "    Events.publish(\"hit\", { damage = 1, tags = { \"a\", \"b\" } })"));
    const ECS::Entity listener = world.AddScripted("Scripts/Listener.lua");
    (void)world.AddScripted("Scripts/Shooter.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("got hit 1 b").size() == 1);
    CHECK(world.Scripts->GetSubscriptionCount() == 1);

    world.Context.GetSceneManager().DeleteGameObject(listener);
    CHECK(world.Scripts->GetSubscriptionCount() == 0);
    world.Frame(STEP);
    CHECK(log.Lines("got hit").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Script events - a handler error names the subscriber's entity and file, and the other handlers run")
{
    EngineWorld world;
    world.WriteScript("Broken.lua", Script("Broken", "    Events.subscribe(\"go\", function() error(\"bad handler\") end)"));
    world.WriteScript("Fine.lua", Script("Fine", "    Events.subscribe(\"go\", function() print(\"fine got go\") end)",
                                         "    Events.publish(\"go\")"));
    const ECS::Entity broken = world.AddScripted("Scripts/Broken.lua");
    (void)world.AddScripted("Scripts/Fine.lua");
    const std::string brokenName = world.Ecs().GetComponent<ECS::HierarchyComponent>(broken).Name;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 2); // one per frame: a handler error does not fault the script
    CHECK(errors[0].find("[Script] " + brokenName + " (assets://Scripts/Broken.lua)") != std::string::npos);
    CHECK(errors[0].find("handler for event 'go'") != std::string::npos);
    CHECK(log.Text().find("bad handler") != std::string::npos);
    CHECK(log.Lines("fine got go").size() == 2);
    REQUIRE(world.Stop());
}

TEST_CASE("Script events - no subscription survives Stop, and subscribing outside a method is an error")
{
    EngineWorld world;
    world.WriteScript("Listener.lua", Script("Listener", LISTENER));
    world.WriteScript("Early.lua", "local ok = pcall(Events.subscribe, \"x\", print)\nprint(\"top level subscribe ok \" .. tostring(ok))\n" +
                                       Script("Early", ""));
    (void)world.AddScripted("Scripts/Listener.lua");
    (void)world.AddScripted("Scripts/Early.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Scripts->GetSubscriptionCount() == 1);
    CHECK(log.Lines("top level subscribe ok false").size() == 1);
    REQUIRE(world.Stop());
    CHECK(world.Scripts->GetSubscriptionCount() == 0);
}

TEST_CASE("Script events - an event a handler publishes is delivered next frame, so there is no loop")
{
    EngineWorld world;
    world.WriteScript("Echo.lua", Script("Echo", R"lua(
    count = 0
    Events.subscribe("echo", function()
        count = count + 1
        print("echo " .. count)
        Events.publish("echo")
    end)
    Events.publish("echo")
)lua"));
    (void)world.AddScripted("Scripts/Echo.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(Said(log) == std::vector<std::string>{ "echo 1" });
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(Said(log) == std::vector<std::string>{ "echo 1", "echo 2", "echo 3" });
    REQUIRE(world.Stop());
}
