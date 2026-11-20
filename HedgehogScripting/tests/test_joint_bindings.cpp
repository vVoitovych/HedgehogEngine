#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/JointComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>

using HedgehogEngine::ColliderComponent;
using HedgehogEngine::JointComponent;
using HedgehogEngine::RigidBodyComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnStart()\n" + onStart + "\nend\n" + "function " + name +
               ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
    }

    struct JointWorld : EngineWorld
    {
        void Step(int count = 1)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        ECS::Entity Box(const std::string& name, const HM::Vector3& position, const HM::Vector3& scale)
        {
            const ECS::Entity entity = Context.GetSceneManager().CreateGameObject();
            Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
            auto& transform    = Ecs().GetComponent<TransformComponent>(entity);
            transform.Position = position;
            transform.Scale    = scale;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
            Ecs().AddComponent(entity, ColliderComponent{});
            return entity;
        }

        // A weightless 1.8 m door beside a static post named Post, scripted with script.
        ECS::Entity Door(const std::string& script)
        {
            (void)Box("Post", HM::Vector3(0.0f, 0.0f, 2.0f), HM::Vector3(0.1f, 0.1f, 1.0f));
            const ECS::Entity door = Box("Door", HM::Vector3(1.05f, 0.0f, 2.0f), HM::Vector3(0.9f, 0.04f, 1.0f));
            RigidBodyComponent rigid;
            rigid.GravityScale = 0.0f;
            Ecs().AddComponent(door, rigid);
            Ecs().AddComponent(door, HedgehogEngine::ScriptComponent{});
            auto& component      = Ecs().GetComponent<HedgehogEngine::ScriptComponent>(door);
            component.ScriptPath = script;
            component.Enable     = true;
            return door;
        }
    };

    // OnStart's hinge of the door about +Z at its left edge to the post, limited to +-60 degrees.
    const std::string HINGE = "local joint = self.entity:addJoint()\n"
                              "Log.info('before', self.entity:hasJoint(), joint.angle)\n"
                              "joint.type = JointType.Hinge\n"
                              "joint.connectedEntity = Scene.find('Post')\n"
                              "joint.anchor = Vector3(-1, 0, 0)\n"
                              "joint.axis = Vector3(0, 0, 1)\n"
                              "joint.limitsEnabled = true\n"
                              "joint.minLimit = -60\n"
                              "joint.maxLimit = 60\n"
                              "self.joint = joint\n"
                              "self.frame = 0";
}

TEST_CASE("Joint bindings - a script hinges a door in OnStart, reads its swing and sets a limit at once")
{
    JointWorld world;
    world.WriteScript("Swing.lua", Script("Swing", HINGE,
        "self.frame = self.frame + 1\n"
        "if self.frame == 2 then self.entity:getRigidBody():addImpulse(Vector3(0, 1.5, 0)) end\n"
        "if self.frame == 20 then Log.info('swung', math.abs(self.joint.angle) > 20,"
        " self.joint.connectedEntity.name, self.joint.type == JointType.Hinge, self.joint.maxLimit) end"));
    const ECS::Entity door = world.Door("Scripts/Swing.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("before true 0.0").size() == 1);
    CHECK(world.Context.GetPhysicsSystem()->GetJoint(door).IsSet());
    CHECK(world.Ecs().GetComponent<JointComponent>(door).MaxLimit == 60.0f);
    world.Step(20);
    CHECK(log.Lines("swung true Post true 60.0").size() == 1);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
    CHECK_FALSE(world.Ecs().HasComponent<JointComponent>(door));

    // Limited to 10 degrees on its first frame, before it is pushed, it swings no further.
    JointWorld limited;
    limited.WriteScript("Tight.lua", Script("Tight", HINGE,
        "self.frame = self.frame + 1\n"
        "if self.frame == 1 then self.joint.maxLimit = 10; self.joint.minLimit = -10 end\n"
        "if self.frame == 2 then self.entity:getRigidBody():addImpulse(Vector3(0, 1.5, 0)) end\n"
        "self.widest = math.max(self.widest or 0, math.abs(self.joint.angle))\n"
        "if self.frame == 40 then Log.info('widest', self.widest < 12, self.widest > 5) end"));
    (void)limited.Door("Scripts/Tight.lua");
    LogCapture tight;
    REQUIRE(limited.Context.Play());
    limited.Step(41);
    CHECK(tight.Lines("widest true true").size() == 1);
    REQUIRE(limited.Stop());
}

TEST_CASE("Joint bindings - bad values are script errors; angle and position are 0 outside a joint")
{
    JointWorld world;
    world.WriteScript("Bad.lua", Script("Bad",
        "self.joint = self.entity:addJoint()\n"
        "Log.info('none', self.entity:getComponent('JointComponent') ~= nil, self.joint.connectedEntity)\n"
        "self.gone = Scene.spawn('Gone')\n"
        "self.gone:destroy()",
        "if self.tried then return end\n"
        "self.tried = true\n"
        "local joint = self.joint\n"
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('type', function() joint.type = 9 end)\n"
        "try('axis', function() joint.axis = Vector3(0 / 0, 0, 1) end)\n"
        "try('stale', function() joint.connectedEntity = self.gone end)\n"
        "try('kind', function() joint.connectedEntity = 3 end)\n"
        "try('spring', function() joint.springFrequency = -1 end)\n"
        "Log.info('slider', joint.position, joint.angle)"));
    const ECS::Entity box = world.Box("Box", HM::Vector3(0.0f, 0.0f, 5.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    world.Ecs().AddComponent(box, RigidBodyComponent{});
    world.Ecs().AddComponent(box, HedgehogEngine::ScriptComponent{});
    world.Ecs().GetComponent<HedgehogEngine::ScriptComponent>(box).ScriptPath = "Scripts/Bad.lua";
    world.Ecs().GetComponent<HedgehogEngine::ScriptComponent>(box).Enable     = true;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(2);
    CHECK(log.Lines("none true nil").size() == 1);
    for (const char* label : { "type false", "axis false", "stale false", "kind false", "spring false" })
    {
        CAPTURE(label);
        CHECK(log.Lines(label).size() == 1);
    }
    CHECK(log.Lines("9 is not a JointType").size() == 1);
    CHECK(log.Lines("a joint's axis must be a finite Vector3").size() == 1);
    CHECK(log.Lines("which no longer exists").size() == 1);
    CHECK(log.Lines("slider 0.0 0.0").size() == 1);
    REQUIRE(world.Stop());
}
