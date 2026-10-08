#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/PhysicsEvents.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Events/UiEvents.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Hears every engine event the script system forwards, and saves a state of its own.
    const std::string LISTENER = R"lua(
Listener = setmetatable({}, { __index = ActorScript })
Listener.__index = Listener
function Listener:new() return setmetatable(ActorScript:new(), Listener) end
function Listener:OnStart()
    Events.subscribe("AnimationFinished", function() print("heard animation") end)
    Events.subscribe("UiButtonClicked", function() print("heard click") end)
    Events.subscribe("GameLoaded", function() print("heard load") end)
end
function Listener:OnUpdate(dt) print("updated") end
function Listener:OnSave() print("saved state") return { value = 1 } end
function Listener:OnDestroy() print("destroyed") end
)lua";

    struct UnregisterWorld
    {
        TempDir     Saves;
        EngineWorld World;
        ECS::Entity Listener   = 0;
        int         EngineRemovals = 0;

        UnregisterWorld()
        {
            World.Context.GetSaveGames().SetSaveDirectory(Saves.Path());
            World.WriteScript("Listener.lua", LISTENER);
            Listener = World.AddScripted("Scripts/Listener.lua");
            // Stands in for the engine's own removal callback, which the script system chains.
            World.Ecs().SetComponentRemovedCallback<HedgehogEngine::ScriptComponent>(
                [this](ECS::Entity, HedgehogEngine::ScriptComponent&) { ++EngineRemovals; });
        }

        void PublishAll()
        {
            HedgehogEngine::EventBus& bus = World.Context.GetEventBus();
            bus.Publish(HedgehogEngine::AnimationFinishedEvent{ Listener, "Clip" });
            bus.Publish(HedgehogEngine::UiButtonClickedEvent{ Listener });
            bus.Publish(HedgehogEngine::GameLoadedEvent{ "slot" });
        }

        // The sections of a slot written at the end of one Playing frame.
        EcsSerialization::SaveGameFile SaveSlot(const std::string& slot)
        {
            REQUIRE(World.Context.GetSaveGames().RequestSave(slot));
            World.Context.UpdatePlayMode(STEP);
            std::ifstream     in(Saves.Path() / (slot + ".save"));
            std::stringstream text;
            text << in.rdbuf();
            auto save = EcsSerialization::ReadSaveGame(text.str());
            REQUIRE(save.Value.has_value());
            return *save.Value;
        }
    };
}

TEST_CASE("ScriptSystem unregister - mid-Play the scripts end, and no engine event or save reaches them")
{
    UnregisterWorld world;
    LogCapture      log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    CHECK(world.SaveSlot("before").Sections.count(EcsSerialization::SAVE_SECTION_SCRIPTS) == 1);
    CHECK(log.Lines("saved state").size() == 1);

    REQUIRE(world.World.Ecs().UnregisterSystem<HedgehogScripting::ScriptSystem>());
    CHECK(log.Lines("destroyed").size() == 1);

    const HedgehogEngine::EventBus& bus = world.World.Context.GetEventBus();
    CHECK(bus.GetSubscriberCount<HedgehogEngine::AnimationFinishedEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::UiButtonClickedEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::GameLoadedEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::CollisionEnterEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::CollisionExitEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::TriggerEnterEvent>() == 0);
    CHECK(bus.GetSubscriberCount<HedgehogEngine::TriggerExitEvent>() == 0);

    world.PublishAll();
    world.World.Frame(STEP);
    const EcsSerialization::SaveGameFile after = world.SaveSlot("after");
    CHECK(after.Sections.count(EcsSerialization::SAVE_SECTION_WORLD) == 1);
    CHECK(after.Sections.count(EcsSerialization::SAVE_SECTION_SCRIPTS) == 0);

    CHECK(log.Lines("heard").empty());
    CHECK(log.Lines("updated").size() == 2); // the two frames before the unregister
    CHECK(log.Lines("saved state").size() == 1);
    CHECK(log.Lines("[Lua Error]").empty());

    // Only the engine's own callback runs now.
    world.World.Ecs().RemoveComponent<HedgehogEngine::ScriptComponent>(world.Listener);
    CHECK(world.EngineRemovals == 1);
    CHECK(log.Lines("destroyed").size() == 1);
    REQUIRE(world.World.Stop());
}

TEST_CASE("ScriptSystem unregister - in Edit mode it lets go of the events, the section and nothing else")
{
    UnregisterWorld world;
    LogCapture      log;
    REQUIRE(world.World.Ecs().UnregisterSystem<HedgehogScripting::ScriptSystem>());
    CHECK(world.World.Context.GetEventBus().GetSubscriberCount<HedgehogEngine::GameLoadedEvent>() == 0);

    // Play runs no script now, and a save has no Scripts section.
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    world.PublishAll();
    CHECK(world.SaveSlot("slot").Sections.count(EcsSerialization::SAVE_SECTION_SCRIPTS) == 0);
    CHECK(log.Lines("updated").empty());
    CHECK(log.Lines("heard").empty());

    world.World.Ecs().RemoveComponent<HedgehogEngine::ScriptComponent>(world.Listener);
    CHECK(world.EngineRemovals == 1);
    REQUIRE(world.World.Stop());
}
