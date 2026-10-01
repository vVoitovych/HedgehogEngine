#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

namespace
{
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

    // Waits half a second and says on which OnUpdate it woke.
    const std::string HALF_SECOND = R"lua(
    startCoroutine(function()
        print("waiting from " .. Time.frame)
        wait(0.5)
        print("woke at " .. Time.frame)
    end)
)lua";

    // Frames of 0.05 s, so half a second is 10 of them at timeScale 1.
    constexpr float FRAME = 0.05f;
}

TEST_CASE("Coroutines - wait(0.5) counts scaled time: 10 frames of 0.05 s, 5 at timeScale 2")
{
    for (const float scale : { 1.0f, 2.0f })
    {
        EngineWorld world;
        world.WriteScript("Sleeper.lua", Script("Sleeper", HALF_SECOND));
        (void)world.AddScripted("Scripts/Sleeper.lua");

        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Context.GetFixedStepClock().TimeScale = scale;
        for (int frame = 0; frame < 12; ++frame)
            world.Frame(FRAME);
        // It first runs in frame 1's pass, after OnUpdate.
        const std::vector<std::string> expected = { "waiting from 1", scale == 1.0f ? "woke at 11" : "woke at 6" };
        CHECK(Said(log) == expected);
        CHECK(world.Scripts->GetCoroutineCount() == 0);
        CHECK(log.Lines("[ERROR]").empty());
        REQUIRE(world.Stop());
    }
}

TEST_CASE("Coroutines - Pause freezes wait: the paused frames do not count")
{
    EngineWorld world;
    world.WriteScript("Sleeper.lua", Script("Sleeper", HALF_SECOND));
    (void)world.AddScripted("Scripts/Sleeper.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int frame = 0; frame < 5; ++frame)
        world.Frame(FRAME);
    REQUIRE(world.Context.Pause());
    for (int frame = 0; frame < 30; ++frame)
        world.Frame(FRAME);
    CHECK(Said(log) == std::vector<std::string>{ "waiting from 1" });
    CHECK(world.Scripts->GetCoroutineCount() == 1);

    REQUIRE(world.Context.Resume());
    for (int frame = 0; frame < 5; ++frame)
        world.Frame(FRAME);
    CHECK(Said(log) == std::vector<std::string>{ "waiting from 1" });
    world.Frame(FRAME);
    CHECK(Said(log) == std::vector<std::string>{ "waiting from 1", "woke at 11" });
    REQUIRE(world.Stop());
}

TEST_CASE("Coroutines - waitFrames(3) resumes 3 OnUpdates later, waitUntil on the frame its predicate holds")
{
    EngineWorld world;
    world.WriteScript("Counter.lua", Script("Counter", R"lua(
    startCoroutine(function()
        print("frames from " .. Time.frame)
        waitFrames(3)
        print("frames to " .. Time.frame)
        coroutine.yield()
        print("a plain yield to " .. Time.frame)
    end)
    startCoroutine(function()
        waitUntil(function() return ready end)
        print("until at " .. Time.frame)
    end)
)lua",
                                              "    if Time.frame == 7 then ready = true end"));
    (void)world.AddScripted("Scripts/Counter.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int frame = 0; frame < 10; ++frame)
        world.Frame(1.0f / 60.0f);
    const std::vector<std::string> expected = { "frames from 1", "frames to 4", "a plain yield to 5", "until at 7" };
    CHECK(Said(log) == expected);
    CHECK(world.Scripts->GetCoroutineCount() == 0);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Coroutines - wait outside a coroutine is an error naming the script")
{
    EngineWorld world;
    world.WriteScript("Outside.lua", Script("Outside", "    wait(1)"));
    const ECS::Entity entity = world.AddScripted("Scripts/Outside.lua");
    const std::string name   = world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("[Script] " + name + " (assets://Scripts/Outside.lua)") != std::string::npos);
    CHECK(errors[0].find("assets://Scripts/Outside.lua:5: wait can only be called inside a coroutine") != std::string::npos);
    REQUIRE(world.Stop());
}

TEST_CASE("Coroutines - an error logs the entity, file and traceback, and the script carries on")
{
    EngineWorld world;
    world.WriteScript("Faulty.lua", Script("Faulty", R"lua(
    label = "mine"
    startCoroutine(function()
        print("coroutine sees " .. label)
        waitFrames(1)
        error("coroutine broke")
    end)
)lua",
                                            "    print(\"update \" .. Time.frame)"));
    const ECS::Entity entity = world.AddScripted("Scripts/Faulty.lua");
    const std::string name   = world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name;

    LogCapture log;
    REQUIRE(world.Context.Play());
    for (int frame = 0; frame < 3; ++frame)
        world.Frame(1.0f / 60.0f);
    const std::vector<std::string> expected = { "update 1", "coroutine sees mine", "update 2", "update 3" };
    CHECK(Said(log) == expected);
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("[Script] " + name + " (assets://Scripts/Faulty.lua): coroutine:") != std::string::npos);
    CHECK(errors[0].find("coroutine broke") != std::string::npos);
    CHECK(log.Text().find("stack traceback") != std::string::npos);
    CHECK(world.Scripts->GetCoroutineCount() == 0);
    REQUIRE(world.Stop());
}

TEST_CASE("Coroutines - stopCoroutine ends one; destroying the entity or Stop ends the rest")
{
    EngineWorld world;
    const std::string forever = R"lua(
    kept = startCoroutine(function() while true do waitFrames(1) end end)
    stopped = startCoroutine(function() while true do waitFrames(1) end end)
    print("stopped " .. tostring(stopCoroutine(stopped)) .. " again " .. tostring(stopCoroutine(stopped)))
)lua";
    world.WriteScript("Forever.lua", Script("Forever", forever));
    const ECS::Entity doomed = world.AddScripted("Scripts/Forever.lua");
    (void)world.AddScripted("Scripts/Forever.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    world.Frame(1.0f / 60.0f);
    CHECK(log.Lines("stopped true again false").size() == 2);
    CHECK(world.Scripts->GetCoroutineCount() == 2);

    world.Context.GetSceneManager().DeleteGameObject(doomed);
    CHECK(world.Scripts->GetCoroutineCount() == 1);
    world.Frame(1.0f / 60.0f);
    CHECK(world.Scripts->GetCoroutineCount() == 1);
    REQUIRE(world.Stop());
    CHECK(world.Scripts->GetCoroutineCount() == 0);
    CHECK(log.Lines("[ERROR]").empty());
}
