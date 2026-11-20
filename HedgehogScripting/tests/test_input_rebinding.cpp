#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogInput/api/InputActionMap.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <string>

namespace
{
    constexpr float DT = 1.0f / 60.0f;

    // A script class named Rebind whose OnStart and OnUpdate run the given bodies.
    std::string Script(const std::string& onStart, const std::string& onUpdate)
    {
        return "Rebind = setmetatable({}, { __index = ActorScript })\nRebind.__index = Rebind\n"
               "function Rebind:new() return setmetatable(ActorScript:new(), Rebind) end\n"
               "function Rebind:OnStart()\n" + onStart + "\nend\n"
               "function Rebind:OnUpdate(dt)\n" + onUpdate + "\nend\n";
    }

    HW::RawInput Keys(std::initializer_list<HW::Key> keys)
    {
        HW::RawInput input;
        for (const HW::Key key : keys)
            input.Keys[static_cast<size_t>(key)] = true;
        return input;
    }

    // A world whose player:// is dir, running the Rebind script.
    struct RebindWorld
    {
        EngineWorld World;

        RebindWorld(const TempDir& dir, const std::string& onStart, const std::string& onUpdate = "")
        {
            REQUIRE(FS::MountPlayer(World.Context.GetFileSystem(), dir.Path()));
            World.WriteScript("Rebind.lua", Script(onStart, onUpdate));
            (void)World.AddScripted("Scripts/Rebind.lua");
            REQUIRE(World.Context.Play());
        }
    };

    const std::string HEAR_REBOUND = R"lua(
    Events.subscribe("InputRebound", function(p)
        print("rebound", p.action, p.index, tostring(p.cancelled), p.binding and p.binding.source,
              p.binding and p.binding.name)
    end)
)lua";
}

TEST_CASE("Input rebinding - listening hears the next press, and that key then presses the action")
{
    TempDir     player;
    RebindWorld world(player, HEAR_REBOUND + "Input.listen('UiSubmit', 1)\nprint('listening', Input.isListening())",
                      "print('submit', Input.isDown('UiSubmit'), Input.isListening())");
    LogCapture  log;
    world.World.Frame(DT, Keys({}));
    world.World.Frame(DT, Keys({ HW::Key::F }));
    CHECK(log.Lines("listening true").size() == 1);
    CHECK(log.Lines("rebound UiSubmit 1 nil Key F").size() == 1);
    CHECK(log.Lines("submit false false").size() == 1); // the press that ends listening is consumed

    world.World.Frame(DT, Keys({}));
    world.World.Frame(DT, Keys({ HW::Key::F }));
    CHECK(log.Lines("submit true false").size() == 1);
    world.World.Frame(DT, Keys({}));
    world.World.Frame(DT, Keys({ HW::Key::Enter }));
    CHECK(log.Lines("submit true false").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Input rebinding - Escape cancels, and while listening every action reads up")
{
    TempDir     player;
    RebindWorld world(player, HEAR_REBOUND + "Input.listen('UiPointerPress', 1)",
                      "print('submit', Input.isDown('UiSubmit'))\n"
                      "print('first', Input.getBindings('UiPointerPress')[1].name)");
    LogCapture  log;
    world.World.Frame(DT, Keys({}));
    world.World.Frame(DT, Keys({ HW::Key::Enter })); // a key is not a mouse button: still listening
    CHECK(log.Lines("submit true").empty());
    world.World.Frame(DT, Keys({ HW::Key::Enter, HW::Key::Escape }));
    CHECK(log.Lines("rebound UiPointerPress 1 true nil nil").size() == 1);
    CHECK(world.World.Context.GetInputOverrides().Actions.empty());
    CHECK(log.Lines("first Left").size() == 3);

    world.World.Frame(DT, Keys({}));
    world.World.Frame(DT, Keys({ HW::Key::Enter }));
    CHECK(log.Lines("submit true").size() == 1);
}

TEST_CASE("Input rebinding - setBinding and saveBindings write what a new engine reads back")
{
    TempDir player;
    {
        RebindWorld world(player, "Input.setBinding('UiSubmit', 1, { source = 'Key', name = 'G' })\n"
                                  "local b = Input.getBindings('UiSubmit')[1]\n"
                                  "print('set', b.source, b.name, b.scale, b.deadzone)\n"
                                  "print('saved', Input.saveBindings())");
        LogCapture log;
        world.World.Frame(DT, Keys({}));
        CHECK(log.Lines("set Key G 1.0 0.0").size() == 1);
        CHECK(log.Lines("saved true").size() == 1);
    }

    EngineWorld again;
    REQUIRE(FS::MountPlayer(again.Context.GetFileSystem(), player.Path()));
    REQUIRE(again.Context.LoadInputOverrides());
    const std::optional<size_t> submit = HInput::FindAction(again.Context.GetInputActions().Game, "UiSubmit");
    REQUIRE(submit);
    CHECK(again.Context.GetInputActions().Game.Actions[*submit].Bindings[0].Code ==
          static_cast<uint16_t>(HW::Key::G));

    // resetBindings brings the project's back.
    TempDir     other;
    RebindWorld world(other, "Input.setBinding('UiSubmit', 1, { source = 'Key', name = 'G' })\n"
                             "Input.resetBindings('UiSubmit')\n"
                             "print('reset', Input.getBindings('UiSubmit')[1].name)");
    LogCapture  log;
    world.World.Frame(DT, Keys({}));
    CHECK(log.Lines("reset Enter").size() == 1);
}

TEST_CASE("Input rebinding - unknown actions, bad indices and malformed bindings are script errors")
{
    const std::pair<std::string, std::string> cases[] = {
        { "Input.getBindings('Fly')", "no action 'Fly'" },
        { "Input.listen('UiSubmit', 9)", "Input.listen: UiSubmit has no binding 9" },
        { "Input.setBinding('UiSubmit', 0, { source = 'Key', name = 'G' })", "UiSubmit has no binding 0" },
        { "Input.setBinding('UiSubmit', 1, 'G')", "Input.setBinding: the binding must be a table" },
        { "Input.setBinding('UiSubmit', 1, { source = 'Joystick' })", "source 'Joystick' is not a binding source" },
        { "Input.setBinding('UiSubmit', 1, { source = 'Key', name = 'NoKey' })", "'NoKey' names no Key control" },
        { "Input.setBinding('UiSubmit', 1, { source = 'Key' })", "the binding's name must be a string" },
        { "Input.setBinding('UiSubmit', 1, { source = 'Key', name = 'G', scale = 'x' })",
          "the binding's scale must be a finite number" },
        { "Input.resetBindings('Fly')", "no action 'Fly'" },
    };
    for (const auto& [statement, message] : cases)
    {
        TempDir     player;
        RebindWorld world(player, "local ok, err = pcall(function() " + statement + " end)\n"
                                  "print('ok', ok, tostring(err))");
        LogCapture  log;
        world.World.Frame(DT, Keys({}));
        INFO(statement);
        CHECK(log.Lines("ok false").size() == 1);
        CHECK(log.Lines(message).size() == 1);
        CHECK(world.World.Context.GetInputOverrides().Actions.empty());
    }
}

TEST_CASE("Input rebinding - outside Play nothing changes and the first change warns")
{
    EngineWorld world;
    world.WriteScript("TopLevel.lua", "Input.setBinding('UiSubmit', 1, { source = 'Key', name = 'G' })\n"
                                      "Input.listen('UiSubmit', 1)\n"
                                      "print('described', Input.isListening(), Input.saveBindings())\n");
    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK(log.Lines("did nothing").size() == 1);
    CHECK(log.Lines("described false false").size() == 1);
    CHECK(world.Context.GetInputOverrides().Actions.empty());
}
