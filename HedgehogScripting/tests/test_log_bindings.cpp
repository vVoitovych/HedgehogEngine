#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Lines 1 to 3 define the class; OnStart's body starts on line 5.
    std::string ScriptWithOnStart(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnStart()\n" + body + "\nend\n";
    }

    void PlayOneFrame(EngineWorld& world)
    {
        REQUIRE(world.Context.Play());
        world.Context.UpdatePlayMode(STEP);
        REQUIRE(world.Stop());
    }
}

TEST_CASE("Log bindings - Log.warn gives one warning prefixed with the script's file and line")
{
    EngineWorld world;
    world.WriteScript("Warner.lua", ScriptWithOnStart("Warner", "    Log.warn('x')"));
    (void)world.AddScripted("Scripts/Warner.lua");

    LogCapture log;
    PlayOneFrame(world);

    const auto warnings = log.Lines("[WARNING]");
    REQUIRE(warnings.size() == 1);
    CHECK(warnings[0].rfind("[WARNING][Script] assets://Scripts/Warner.lua:5: x", 0) == 0);
}

TEST_CASE("Log bindings - Log.info and print are info lines, Log.error an error line")
{
    EngineWorld world;
    world.WriteScript("Talker.lua", ScriptWithOnStart("Talker", "    Log.info('told', 1, true)\n"
                                                                "    print('printed')\n"
                                                                "    Log.error('broken')"));
    (void)world.AddScripted("Scripts/Talker.lua");

    LogCapture log;
    PlayOneFrame(world);

    CHECK(log.Lines("[INFO][Script] assets://Scripts/Talker.lua:5: told 1 true").size() == 1);
    CHECK(log.Lines("[INFO][Script] assets://Scripts/Talker.lua:6: printed").size() == 1);
    CHECK(log.Lines("[ERROR][Script] assets://Scripts/Talker.lua:7: broken").size() == 1);
}

TEST_CASE("Log bindings - a failing __tostring inside a log call is a script error and logs nothing")
{
    EngineWorld world;
    world.WriteScript("BadLog.lua",
                      ScriptWithOnStart("BadLog", "    Log.info(setmetatable({}, { __tostring = function() error('no text') end }))"));
    (void)world.AddScripted("Scripts/BadLog.lua");

    LogCapture log;
    PlayOneFrame(world);

    CHECK(log.Lines("[INFO][Script]").empty());
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(log.Text().find("no text") != std::string::npos);
}
