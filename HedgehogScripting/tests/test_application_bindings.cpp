#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

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

    // Quits on its tenth OnUpdate, so a second Play shows the frame counter restarting.
    const std::string QUITTER = R"lua(
Quitter = setmetatable({}, { __index = ActorScript })
Quitter.__index = Quitter
function Quitter:new() return setmetatable(ActorScript:new(), Quitter) end
function Quitter:OnUpdate(dt)
    if Time.frame == 10 then Application.quit() end
end
)lua";
}

TEST_CASE("Application bindings - quit sets the flag on its frame, and Play and Stop clear it")
{
    EngineWorld world;
    world.WriteScript("Quitter.lua", QUITTER);
    (void)world.AddScripted("Scripts/Quitter.lua");
    auto& requests = world.Context.GetApplicationRequests();

    for (int run = 0; run < 2; ++run)
    {
        REQUIRE(world.Context.Play());
        CHECK_FALSE(requests.QuitRequested);
        for (int frame = 1; frame <= 9; ++frame)
        {
            world.Frame(STEP);
            CHECK_FALSE(requests.QuitRequested);
        }
        world.Frame(STEP);
        CHECK(requests.QuitRequested);
        CHECK(world.Context.GetPlayState() == HedgehogEngine::PlayState::Playing);
        REQUIRE(world.Stop());
        CHECK_FALSE(requests.QuitRequested);
    }
}

TEST_CASE("Application bindings - Play clears a flag set before it")
{
    EngineWorld world;
    world.Context.GetApplicationRequests().QuitRequested = true;
    REQUIRE(world.Context.Play());
    CHECK_FALSE(world.Context.GetApplicationRequests().QuitRequested);
}

TEST_CASE("Application bindings - quit outside Play warns once and does nothing")
{
    EngineWorld world;
    world.WriteScript("TopLevel.lua", "Application.quit()\nApplication.quit()\n");

    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK_FALSE(world.Context.GetApplicationRequests().QuitRequested);
    CHECK(log.Lines("Application.quit did nothing").size() == 1);
}

TEST_CASE("Application bindings - isEditor is read live")
{
    EngineWorld world;
    world.WriteScript("Probe.lua", Script("Probe", "function Probe:OnUpdate(dt) "
                                                   "print('isEditor ' .. tostring(Application.isEditor)) end"));
    (void)world.AddScripted("Scripts/Probe.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Context.GetApplicationRequests().IsEditor = true;
    world.Frame(STEP);
    CHECK(log.Lines("isEditor false").size() == 1);
    CHECK(log.Lines("isEditor true").size() == 1);
}

TEST_CASE("Application bindings - every field is read-only, and other keys read nil")
{
    for (const std::string statement : { "Application.isEditor = false", "Application.quit = nil",
                                         "Application.other = 1" })
    {
        EngineWorld world;
        world.WriteScript("Writer.lua", Script("Writer", "function Writer:OnStart() " + statement + " end"));
        (void)world.AddScripted("Scripts/Writer.lua");
        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Frame(STEP);
        INFO(statement);
        CHECK(log.Lines("is read-only").size() == 1);
    }

    EngineWorld world;
    world.WriteScript("Reader.lua", Script("Reader", "function Reader:OnStart() "
                                                     "print('keys ' .. tostring(Application[1]) .. "
                                                     "tostring(Application.other) .. type(Application.quit)) end"));
    (void)world.AddScripted("Scripts/Reader.lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("keys nilnilfunction").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Application bindings - a quit in OnStart survives Play, one in OnDestroy at Stop only warns")
{
    EngineWorld world;
    world.WriteScript("Both.lua", Script("Both", "function Both:OnStart() Application.quit() end\n"
                                                 "function Both:OnDestroy() Application.quit() end"));
    (void)world.AddScripted("Scripts/Both.lua");
    auto& requests = world.Context.GetApplicationRequests();

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(requests.QuitRequested);
    REQUIRE(world.Stop());
    CHECK_FALSE(requests.QuitRequested);
    CHECK(log.Lines("Application.quit did nothing").size() == 1);
}
