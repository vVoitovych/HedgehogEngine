#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogInput/api/ActionState.hpp"

#include <string>
#include <vector>

namespace
{
    constexpr float DT = 1.0f / 60.0f;

    // A script class named `name` whose OnUpdate runs body.
    std::string Script(const std::string& name, const std::string& body)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnUpdate(dt)\n" + body + "\nend\n";
    }

    // What the test's scripts printed, without the "[INFO][Script] <file>:<line>: " prefix.
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

    HW::RawInput Keys(std::initializer_list<HW::Key> keys)
    {
        HW::RawInput input;
        for (const HW::Key key : keys)
            input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }

    const std::string REPORT_SUBMIT = R"lua(
    print(string.format("down %s pressed %s released %s value %.1f", tostring(Input.isDown("UiSubmit")),
        tostring(Input.wasPressed("UiSubmit")), tostring(Input.wasReleased("UiSubmit")), Input.value("UiSubmit")))
)lua";
}

TEST_CASE("Input bindings - a script sees a press, the hold and the release, one frame each")
{
    EngineWorld world;
    world.WriteScript("Submit.lua", Script("Submit", REPORT_SUBMIT));
    (void)world.AddScripted("Scripts/Submit.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(DT, Keys({}));
    world.Frame(DT, Keys({ HW::Key::Enter }));
    world.Frame(DT, Keys({ HW::Key::Enter }));
    world.Frame(DT, Keys({}));
    const std::vector<std::string> expected = {
        "down false pressed false released false value 0.0",
        "down true pressed true released false value 1.0",
        "down true pressed false released false value 1.0",
        "down false pressed false released true value 0.0",
    };
    CHECK(Said(log) == expected);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Input bindings - the pointer reads in game-view pixels")
{
    EngineWorld world;
    world.WriteScript("Pointer.lua", Script("Pointer", R"lua(
    local x, y = Input.pointerPosition()
    local dx, dy = Input.pointerDelta()
    print(string.format("at %.0f %.0f moved %.0f %.0f inside %s", x, y, dx, dy, tostring(Input.isPointerInside())))
)lua"));
    (void)world.AddScripted("Scripts/Pointer.lua");

    HW::RawInput input;
    input.CursorPosition = HM::Vector2(320.0f, 200.0f);
    input.CursorDelta    = HM::Vector2(4.0f, -3.0f);
    input.CursorInside   = true;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(DT, input);
    CHECK(Said(log) == std::vector<std::string>{ "at 320 200 moved 4 -3 inside true" });
    REQUIRE(world.Stop());
}

TEST_CASE("Input bindings - a consumed action reads as up, whether consumed in C++ or by an earlier script")
{
    EngineWorld world;
    world.WriteScript("First.lua", Script("First", R"lua(
    print("first " .. tostring(Input.wasPressed("UiSubmit")))
    Input.consume("UiSubmit")
)lua"));
    world.WriteScript("Second.lua", Script("Second", R"lua(
    print("second " .. tostring(Input.wasPressed("UiSubmit")) .. " " .. tostring(Input.isDown("UiSubmit")))
)lua"));
    (void)world.AddScripted("Scripts/First.lua"); // a lower id: runs first
    (void)world.AddScripted("Scripts/Second.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(DT, Keys({ HW::Key::Space }));
    CHECK(Said(log) == std::vector<std::string>{ "first true", "second false false" });

    // A system that handles the press before the scripts run hides it from all of them.
    world.Context.UpdateGameInput(Keys({}));
    world.Context.UpdateGameInput(Keys({ HW::Key::Enter }));
    const size_t submit = *HInput::FindAction(world.Context.GetInputActions().Game, "UiSubmit");
    HInput::ConsumeAction(world.Context.GetGameActionState(), submit);
    world.Frame(DT);
    const std::vector<std::string> said = Said(log);
    REQUIRE(said.size() == 4);
    CHECK(said[2] == "first false");
    CHECK(said[3] == "second false false");
    REQUIRE(world.Stop());
}

TEST_CASE("Input bindings - an unknown action is a script error naming it, and faults only that script")
{
    EngineWorld world;
    world.WriteScript("Jumper.lua", Script("Jumper", "    print(\"jump \" .. tostring(Input.isDown(\"Jump\")))"));
    world.WriteScript("Steady.lua", Script("Steady", "    print(\"steady\")"));
    (void)world.AddScripted("Scripts/Jumper.lua");
    (void)world.AddScripted("Scripts/Steady.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(DT, Keys({}));
    world.Frame(DT, Keys({}));

    const std::vector<std::string> errors = log.Lines("[ERROR]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("Input: no action 'Jump' in assets://Input/actions.yaml") != std::string::npos);
    CHECK(errors[0].find("assets://Scripts/Jumper.lua") != std::string::npos);
    CHECK(Said(log) == std::vector<std::string>{ "steady", "steady" });

    // A non-string action name is a script error too.
    world.WriteScript("Number.lua", Script("Number", "    Input.isDown(5)"));
    (void)world.AddScripted("Scripts/Number.lua");
    world.Frame(DT, Keys({}));
    CHECK(log.Lines("[ERROR]").size() == 2);
    REQUIRE(world.Stop());
}

TEST_CASE("Input bindings - nothing is down outside Play")
{
    EngineWorld world;
    world.WriteScript("Paused.lua", Script("Paused", REPORT_SUBMIT));
    (void)world.AddScripted("Scripts/Paused.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(DT, Keys({ HW::Key::Enter }));
    REQUIRE(world.Context.Pause());
    world.Frame(DT, Keys({ HW::Key::Enter })); // no OnUpdate while paused
    REQUIRE(world.Context.Resume());
    world.Frame(DT, Keys({ HW::Key::Enter }));
    const std::vector<std::string> said = Said(log);
    REQUIRE(said.size() == 2);
    CHECK(said[0] == "down true pressed true released false value 1.0");
    CHECK(said[1] == "down true pressed true released false value 1.0"); // Pause reset it: a fresh press
    REQUIRE(world.Stop());
}
