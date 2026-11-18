#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogInput/api/GameInputRegion.hpp"

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
}

TEST_CASE("Cursor bindings - a lock in OnStart gives the game a pointer at the centre with deltas")
{
    EngineWorld world;
    world.WriteScript("Locker.lua",
                      Script("Locker",
                             "function Locker:OnStart() Cursor.lock() end\n"
                             "function Locker:OnUpdate(dt)\n"
                             "    local x, y = Input.pointerPosition()\n"
                             "    local dx, dy = Input.pointerDelta()\n"
                             "    print(string.format('locked %s pos %.0f %.0f delta %.0f %.0f inside %s',\n"
                             "        tostring(Cursor.isLocked()), x, y, dx, dy, tostring(Input.isPointerInside())))\n"
                             "end"));
    (void)world.AddScripted("Scripts/Locker.lua");
    auto& requests = world.Context.GetApplicationRequests();

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(requests.Cursor == HW::CursorMode::Locked);

    HW::RawInput source;
    source.CursorPosition = HM::Vector2(10.0f, 20.0f);
    source.CursorDelta    = HM::Vector2(5.0f, -3.0f);
    source.CursorInside   = true;
    source.Focused        = true;

    HInput::GameInputRegion region;
    region.Size            = HM::Vector2(800.0f, 600.0f);
    region.PixelSize       = HM::Vector2(800.0f, 600.0f);
    region.PointerEnabled  = true;
    region.KeyboardEnabled = true;
    region.PointerLocked   = requests.Cursor == HW::CursorMode::Locked;
    HInput::GameInputGate gate;
    world.Frame(STEP, HInput::MakeGameInput(source, region, gate), HM::Vector2(800.0f, 600.0f));
    CHECK(log.Lines("locked true pos 400 300 delta 5 -3 inside true").size() == 1);
}

TEST_CASE("Cursor bindings - mode reads and writes by name")
{
    EngineWorld world;
    world.WriteScript("Modes.lua",
                      Script("Modes",
                             "function Modes:OnStart()\n"
                             "    Cursor.mode = 'Hidden'\n"
                             "    print('a ' .. Cursor.mode)\n"
                             "    Cursor.mode = 'Confined'\n"
                             "    print('b ' .. Cursor.mode)\n"
                             "    Cursor.unlock()\n"
                             "    print('c ' .. Cursor.mode .. ' ' .. tostring(Cursor.isLocked()))\n"
                             "    Cursor.mode = 'Locked'\n"
                             "    print('d ' .. Cursor.mode .. ' ' .. tostring(Cursor.isLocked()))\n"
                             "end"));
    (void)world.AddScripted("Scripts/Modes.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("a Hidden").size() == 1);
    CHECK(log.Lines("b Confined").size() == 1);
    CHECK(log.Lines("c Normal false").size() == 1);
    CHECK(log.Lines("d Locked true").size() == 1);
    CHECK(world.Context.GetApplicationRequests().Cursor == HW::CursorMode::Locked);
}

TEST_CASE("Cursor bindings - Stop and Play reset the request to Normal")
{
    EngineWorld world;
    world.WriteScript("Locker.lua", Script("Locker", "function Locker:OnStart() Cursor.lock() end"));
    (void)world.AddScripted("Scripts/Locker.lua");
    auto& requests = world.Context.GetApplicationRequests();

    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(requests.Cursor == HW::CursorMode::Locked);
    REQUIRE(world.Stop());
    CHECK(requests.Cursor == HW::CursorMode::Normal);

    requests.Cursor = HW::CursorMode::Hidden;
    REQUIRE(world.Context.Play());
    CHECK(requests.Cursor == HW::CursorMode::Normal);
}

TEST_CASE("Cursor bindings - an unknown mode, a non-string and a write to another field are script errors")
{
    for (const std::string statement : { "Cursor.mode = 'Sideways'", "Cursor.mode = 3", "Cursor.mode = nil",
                                         "Cursor.lock = 1", "assert(Cursor[1] == nil) Cursor[1] = 2",
                                         "setmetatable(Cursor, {})" })
    {
        EngineWorld world;
        world.WriteScript("Bad.lua", Script("Bad", "function Bad:OnStart() " + statement + " end"));
        (void)world.AddScripted("Scripts/Bad.lua");
        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Frame(STEP);
        INFO(statement);
        CHECK(world.Context.GetApplicationRequests().Cursor == HW::CursorMode::Normal);
        if (statement == "Cursor.mode = 'Sideways'")
        {
            CHECK(log.Lines("Sideways").size() >= 1);
            CHECK(log.Lines("Normal, Hidden, Locked and Confined").size() == 1);
        }
        else if (statement == "Cursor.mode = 3" || statement == "Cursor.mode = nil")
        {
            CHECK(log.Lines("takes a string").size() == 1);
        }
        else if (statement == "setmetatable(Cursor, {})")
        {
            CHECK(log.Lines("protected metatable").size() == 1);
        }
        else
        {
            CHECK(log.Lines("is read-only").size() == 1);
        }
    }
}

TEST_CASE("Cursor bindings - outside Play a change warns once and does nothing")
{
    EngineWorld world;
    world.WriteScript("TopLevel.lua", "Cursor.lock()\nCursor.mode = 'Hidden'\n");
    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK(world.Context.GetApplicationRequests().Cursor == HW::CursorMode::Normal);
    CHECK(log.Lines("did nothing").size() == 1);

    world.WriteScript("Unknown.lua", "Cursor.mode = 'Sideways'\n");
    LogCapture errors;
    (void)world.Scripts->DescribeScript("Scripts/Unknown.lua");
    CHECK(errors.Lines("Sideways").size() >= 1);
}
