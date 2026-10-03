#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>

using HedgehogEngine::LightComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Moves itself, a lamp's intensity and its own state every frame; saves on frame 2 and loads
    // that save from inside OnUpdate on frame 5.
    const std::string PLAYER = R"lua(
Player = setmetatable({}, { __index = ActorScript })
Player.__index = Player
Properties = { speed = 1.0 }
function Player:new() return setmetatable(ActorScript:new(), Player) end
function Player:OnStart()
    print("started")
    self.count = 0
end
function Player:OnUpdate(dt)
    self.count = self.count + 1
    self.speed = self.speed + 1
    local p = self.entity.transform.position
    self.entity.transform.position = Vector3(p.x + 1, p.y, p.z)
    local light = Scene.find("Lamp"):getLight()
    light.intensity = light.intensity + 1
    if Time.frame == 2 then print("save queued " .. tostring(Save.write("slot1"))) end
    if Time.frame == 5 then print("load queued " .. tostring(Save.load("slot1"))) end
end
function Player:OnSave() return { count = self.count } end
function Player:OnLoad(state)
    self.count = state.count
    print("loaded count " .. self.count .. " speed " .. self.speed)
    Events.subscribe("GameLoaded", function(event) print("game loaded " .. event.slot) end)
end
function Player:OnDestroy() print("destroyed") end
)lua";

    struct SaveWorld
    {
        TempDir     Saves;
        EngineWorld World;
        ECS::Entity Player = 0;
        ECS::Entity Lamp   = 0;

        SaveWorld()
        {
            World.Context.GetSaveGames().SetSaveDirectory(Saves.Path());
            World.WriteScript("Player.lua", PLAYER);
            Player = World.AddScripted("Scripts/Player.lua");
            Lamp   = World.Context.GetSceneManager().CreateGameObject();
            World.Ecs().GetComponent<ECS::HierarchyComponent>(Lamp).Name = "Lamp";
            World.Ecs().AddComponent(Lamp, LightComponent{});
        }

        float PlayerX() { return World.Ecs().GetComponent<TransformComponent>(Player).Position.x(); }
        float Intensity() { return World.Ecs().GetComponent<LightComponent>(Lamp).Intensity; }
    };
}

TEST_CASE("Save bindings - a load from OnUpdate brings back the world, components and script state at the frame's end")
{
    SaveWorld   world;
    const float baseIntensity = world.Intensity();
    const std::string before  = world.World.Context.GetSceneManager().CaptureSnapshot().Yaml;

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    world.World.Frame(STEP); // saves at the end of this frame
    CHECK(log.Lines("save queued true").size() == 1);
    CHECK(log.Lines("[Save] Saved slot 'slot1'.").size() == 1);
    CHECK(std::filesystem::is_regular_file(world.Saves.Path() / "slot1.save"));

    world.World.Frame(STEP);
    world.World.Frame(STEP);
    CHECK(world.PlayerX() == 4.0f);
    world.World.Frame(STEP); // moves to 5, then loads at the end of the frame
    CHECK(log.Lines("load queued true").size() == 1);
    CHECK(world.PlayerX() == 2.0f);
    CHECK(world.Intensity() == baseIntensity + 2.0f);
    CHECK(log.Lines("destroyed").size() == 1); // the old instance went with the old world

    world.World.Frame(STEP); // the loaded instance: OnLoad in place of OnStart, then its first OnUpdate
    CHECK(log.Lines("loaded count 2 speed 3").size() == 1);
    CHECK(log.Lines("started").size() == 1);
    CHECK(log.Lines("game loaded slot1").size() == 1);
    CHECK(world.PlayerX() == 3.0f);
    CHECK(world.Intensity() == baseIntensity + 3.0f);
    CHECK(log.Lines("[ERROR]").empty());

    // Stop restores the scene as it was before Play, whatever was loaded.
    REQUIRE(world.World.Stop());
    CHECK(world.World.Context.GetSceneManager().CaptureSnapshot().Yaml == before);
}

TEST_CASE("Save bindings - list, exists and delete, and what is refused")
{
    SaveWorld world;
    world.World.WriteScript("Slots.lua", R"lua(
Slots = setmetatable({}, { __index = ActorScript })
Slots.__index = Slots
function Slots:new() return setmetatable(ActorScript:new(), Slots) end
function Slots:OnStart()
    Save.write("b")
    Save.write("a")
end
function Slots:OnUpdate(dt)
    if Time.frame ~= 2 then return end
    for _, slot in ipairs(Save.list()) do
        print("slot " .. slot.name .. " v" .. slot.saveVersion .. " data" .. slot.gameDataVersion .. " at " .. slot.timestamp)
    end
    print("exists " .. tostring(Save.exists("a")) .. " " .. tostring(Save.exists("c")))
    print("deleted " .. tostring(Save.delete("b")) .. " then " .. tostring(Save.exists("b")))
    print("missing " .. tostring(Save.load("missing")))
    Save.write("bad name")
end
)lua");
    (void)world.World.AddScripted("Scripts/Slots.lua");

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    world.World.Frame(STEP);
    const auto slots = log.Lines(": slot ");
    REQUIRE(slots.size() == 2);
    CHECK(slots[0].find("slot a v1 data1 at 20") != std::string::npos);
    CHECK(slots[1].find("slot b v1") != std::string::npos);
    CHECK(log.Lines("exists true false").size() == 1);
    CHECK(log.Lines("deleted true then false").size() == 1);
    CHECK(log.Lines("missing false").size() == 1);
    CHECK(log.Lines("[Save] Slot 'missing' does not exist.").size() == 1);
    CHECK(log.Lines("'bad name' is not a save slot name").size() == 1);
    REQUIRE(world.World.Stop());
}

TEST_CASE("Save bindings - nothing is written outside Play or without a save directory")
{
    SaveWorld world;
    world.World.WriteScript("TopLevel.lua", R"lua(
TopLevel = setmetatable({}, { __index = ActorScript })
TopLevel.__index = TopLevel
function TopLevel:new() return setmetatable(ActorScript:new(), TopLevel) end
print("top level write " .. tostring(Save.write("edit")))
)lua");

    LogCapture log;
    (void)world.World.Scripts->DescribeScript("Scripts/TopLevel.lua");
    CHECK(log.Lines("top level write false").size() == 1);
    CHECK(log.Lines("Saves change only in Play mode").size() == 1);
    CHECK_FALSE(std::filesystem::exists(world.Saves.Path() / "edit.save"));

    EngineWorld unset;
    CHECK_FALSE(unset.Context.GetSaveGames().RequestSave("slot"));
    CHECK(log.Lines("[Save] No save directory is set").size() == 1);
}
