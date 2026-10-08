#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

using HedgehogEngine::ColliderComponent;
using HedgehogEngine::ColliderShape;
using HedgehogEngine::RigidBodyComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given methods, each a full `function ... end`.
    std::string Script(const std::string& name, const std::string& methods)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + methods;
    }

    struct EventsWorld : EngineWorld
    {
        void Step(int count = 1)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        // A scripted entity at position with a collider, dynamic (a sphere) or static (a box).
        ECS::Entity Collider(const std::string& script, const HM::Vector3& position, const HM::Vector3& scale,
                             bool dynamic, bool trigger = false)
        {
            const ECS::Entity entity = AddScripted(script);
            TransformComponent& transform = Ecs().GetComponent<TransformComponent>(entity);
            transform.Position = position;
            transform.Scale    = scale;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
            ColliderComponent collider;
            collider.IsTrigger = trigger;
            if (dynamic)
                collider.Shape = ColliderShape::Sphere;
            Ecs().AddComponent(entity, collider);
            if (dynamic)
                Ecs().AddComponent(entity, RigidBodyComponent{});
            return entity;
        }
    };
}

TEST_CASE("Physics script events - a ball landing calls OnCollisionEnter on both scripts, and a subscriber hears both sides")
{
    EventsWorld world;
    world.WriteScript("Floor.lua", Script("Floor",
        "function Floor:OnStart()\n"
        "  Events.subscribe('CollisionEnter', function(payload) Log.info('heard enter', payload.entity.name, payload.other.name, payload.normal.z) end)\n"
        "  Events.subscribe('CollisionExit', function(payload) Log.info('heard exit', payload.entity.name) end)\n"
        "end\n"
        "function Floor:OnCollisionEnter(other, contact) Log.info('floor hit by', other.name, contact.point.z > 0.4) end\n"
        "function Floor:OnUpdate(dt) self.frame = (self.frame or 0) + 1 end\n"));
    world.WriteScript("Ball.lua", Script("Ball",
        "function Ball:OnCollisionEnter(other, contact) Log.info('ball hit', other.name, Time.frame) end\n"
        "function Ball:OnCollisionExit(other) Log.info('ball left', other.name, other:isValid()) end\n"));
    const ECS::Entity floor = world.Collider("Scripts/Floor.lua", HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(50.0f, 50.0f, 0.5f), false);
    const ECS::Entity ball  = world.Collider("Scripts/Ball.lua", HM::Vector3(0.0f, 0.0f, 3.0f), HM::Vector3(1.0f, 1.0f, 1.0f), true);
    world.Ecs().GetComponent<ECS::HierarchyComponent>(floor).Name = "FloorBox";
    world.Ecs().GetComponent<ECS::HierarchyComponent>(ball).Name  = "Ball";

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(120);
    CHECK(log.Lines("floor hit by Ball true").size() == 1);
    CHECK(log.Lines("ball hit FloorBox").size() == 1);
    // Both sides, each with its own normal: up from the floor, down from the ball.
    CHECK(log.Lines("heard enter FloorBox Ball 1.0").size() == 1);
    CHECK(log.Lines("heard enter Ball FloorBox -1.0").size() == 1);
    CHECK(log.Lines("heard enter").size() == 2);

    // The ball's hook ran the frame it landed: its Time.frame is the subscriber's frame too.
    const std::vector<std::string> hit = log.Lines("ball hit FloorBox");
    REQUIRE(hit.size() == 1);
    const std::string line  = hit[0].substr(0, hit[0].find_last_not_of(" \r\n") + 1);
    const int         frame = std::stoi(line.substr(line.rfind(' ') + 1));
    CHECK(frame > 10);
    CHECK(frame < 60);

    // Moving the ball away gives the Exit to both sides.
    world.Ecs().GetComponent<TransformComponent>(ball).Position = HM::Vector3(0.0f, 0.0f, 30.0f);
    world.Step(3);
    CHECK(log.Lines("ball left FloorBox true").size() == 1);
    CHECK(log.Lines("heard exit").size() == 2);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Physics script events - a trigger zone hears a body pass through once each way, and a hook error does not stop the others")
{
    EventsWorld world;
    world.WriteScript("Zone.lua", Script("Zone",
        "function Zone:OnTriggerEnter(other) Log.info('zone entered by', other.name) end\n"
        "function Zone:OnTriggerExit(other) Log.info('zone left by', other.name) end\n"
        "function Zone:OnUpdate(dt) self.alive = true end\n"));
    world.WriteScript("Rock.lua", Script("Rock",
        "function Rock:OnTriggerEnter(other) error('rock broke') end\n"
        "function Rock:OnTriggerExit(other) Log.info('rock out of', other.name) end\n"
        "function Rock:OnUpdate(dt) self.frames = (self.frames or 0) + 1; if self.frames == 150 then Log.info('rock still runs') end end\n"));
    const ECS::Entity zone = world.Collider("Scripts/Zone.lua", HM::Vector3(0.0f, 0.0f, 5.0f), HM::Vector3(1.0f, 1.0f, 1.0f), false, true);
    const ECS::Entity rock = world.Collider("Scripts/Rock.lua", HM::Vector3(0.0f, 0.0f, 10.0f), HM::Vector3(1.0f, 1.0f, 1.0f), true);
    world.Ecs().GetComponent<ECS::HierarchyComponent>(zone).Name = "Zone";
    world.Ecs().GetComponent<ECS::HierarchyComponent>(rock).Name = "Rock";

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(150);
    CHECK(log.Lines("zone entered by Rock").size() == 1);
    CHECK(log.Lines("zone left by Rock").size() == 1);
    // The rock's failing hook is logged with its entity and script, and does not fault it.
    const std::vector<std::string> errors = log.Lines("rock broke");
    REQUIRE(errors.size() >= 1);
    CHECK(errors[0].find("Rock") != std::string::npos);
    CHECK(errors[0].find("assets://Scripts/Rock.lua") != std::string::npos);
    CHECK(errors[0].find("OnTriggerEnter") != std::string::npos);
    CHECK(log.Lines("rock out of Zone").size() == 1);
    CHECK(log.Lines("rock still runs").size() == 1);
    REQUIRE(world.Stop());
}
