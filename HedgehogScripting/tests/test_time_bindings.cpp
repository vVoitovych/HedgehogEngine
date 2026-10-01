#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include <string>
#include <vector>

namespace
{
    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onFixedUpdate,
                       const std::string& onUpdate)
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onFixedUpdate.empty())
            source += "function " + name + ":OnFixedUpdate(dt)\n" + onFixedUpdate + "\nend\n";
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

    // Prints, per hook, what Time reports, rounded so float noise does not matter.
    const std::string FIXED_REPORT = R"lua(
    print(string.format("fixed dt %.3f fixedDeltaTime %.3f deltaTime %.3f", dt, Time.fixedDeltaTime, Time.deltaTime))
)lua";
    const std::string UPDATE_REPORT = R"lua(
    print(string.format("update %d dt %.3f deltaTime %.3f time %.3f", Time.frame, dt, Time.deltaTime, Time.time))
)lua";
}

TEST_CASE("Time bindings - a 1/25 s frame at a 1/50 s step runs two fixed steps, with the dt each hook sees")
{
    EngineWorld world;
    world.WriteScript("Clock.lua", Script("Clock", "", FIXED_REPORT, UPDATE_REPORT));
    (void)world.AddScripted("Scripts/Clock.lua");
    world.Context.GetFixedStepClock().FixedDeltaTime = 1.0f / 50.0f;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 25.0f);
    world.Frame(1.0f / 25.0f);
    const std::vector<std::string> expected = {
        "fixed dt 0.020 fixedDeltaTime 0.020 deltaTime 0.020", "fixed dt 0.020 fixedDeltaTime 0.020 deltaTime 0.020",
        "update 1 dt 0.040 deltaTime 0.040 time 0.040",
        "fixed dt 0.020 fixedDeltaTime 0.020 deltaTime 0.020", "fixed dt 0.020 fixedDeltaTime 0.020 deltaTime 0.020",
        "update 2 dt 0.040 deltaTime 0.040 time 0.080",
    };
    CHECK(Said(log) == expected);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Time bindings - timeScale 0 stops the fixed steps and holds Time.time; OnUpdate runs with deltaTime 0")
{
    EngineWorld world;
    world.WriteScript("Frozen.lua", Script("Frozen", "    Time.timeScale = 0", "    print(\"fixed\")", UPDATE_REPORT));
    (void)world.AddScripted("Scripts/Frozen.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f); // the step for this frame was taken before OnStart set the scale
    world.Frame(1.0f / 60.0f);
    world.Frame(1.0f / 60.0f);
    const std::vector<std::string> expected = {
        "fixed",
        "update 1 dt 0.000 deltaTime 0.000 time 0.017",
        "update 2 dt 0.000 deltaTime 0.000 time 0.017",
        "update 3 dt 0.000 deltaTime 0.000 time 0.017",
    };
    CHECK(Said(log) == expected);
    CHECK(world.Context.GetFixedStepClock().TimeScale == 0.0f);
    REQUIRE(world.Stop());
}

TEST_CASE("Time bindings - timeScale 2 doubles the steps, and values above 100 clamp")
{
    EngineWorld world;
    world.WriteScript("Fast.lua", Script("Fast", R"lua(
    Time.timeScale = 500
    print("clamped " .. Time.timeScale)
    Time.timeScale = 2
)lua",
                                         "    steps = (steps or 0) + 1",
                                         "    print(\"steps \" .. (steps or 0)) steps = 0"));
    (void)world.AddScripted("Scripts/Fast.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f); // one step: the scale was still 1 when this frame's steps were counted
    world.Frame(1.0f / 60.0f);
    world.Frame(1.0f / 60.0f);
    const std::vector<std::string> expected = { "clamped 100.0", "steps 1", "steps 2", "steps 2" };
    CHECK(Said(log) == expected);
    REQUIRE(world.Stop());
}

TEST_CASE("Time bindings - only timeScale is writable, and it must be a finite number")
{
    EngineWorld world;
    world.WriteScript("Writer.lua", Script("Writer", R"lua(
    local okFrame = pcall(function() Time.frame = 5 end)
    local okNan = pcall(function() Time.timeScale = 0/0 end)
    local okText = pcall(function() Time.timeScale = "fast" end)
    print("accepted " .. tostring(okFrame) .. " " .. tostring(okNan) .. " " .. tostring(okText) .. " scale " .. Time.timeScale)
    print("unknown " .. tostring(Time.nothing))
)lua",
                                           "", ""));
    (void)world.AddScripted("Scripts/Writer.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    CHECK(log.Lines("accepted false false false scale 1.0").size() == 1);
    CHECK(log.Lines("unknown nil").size() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Time bindings - Time.time and Time.frame restart at 0 on the next Play")
{
    EngineWorld world;
    world.WriteScript("Clock.lua", Script("Clock", "", "", UPDATE_REPORT));
    (void)world.AddScripted("Scripts/Clock.lua");

    LogCapture log;
    for (int play = 0; play < 2; ++play)
    {
        REQUIRE(world.Context.Play());
        world.Frame(1.0f / 60.0f);
        world.Frame(1.0f / 60.0f);
        REQUIRE(world.Stop());
    }
    const std::vector<std::string> expected = {
        "update 1 dt 0.017 deltaTime 0.017 time 0.017", "update 2 dt 0.017 deltaTime 0.017 time 0.033",
        "update 1 dt 0.017 deltaTime 0.017 time 0.017", "update 2 dt 0.017 deltaTime 0.017 time 0.033",
    };
    CHECK(Said(log) == expected);
}
