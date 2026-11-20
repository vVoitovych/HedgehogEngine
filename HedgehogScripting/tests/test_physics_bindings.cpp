#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include <string>

using HedgehogEngine::ColliderComponent;
using HedgehogEngine::ColliderShape;
using HedgehogEngine::RigidBodyComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onFixedUpdate = "",
                       const std::string& onUpdate = "")
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

    struct PhysicsWorld : EngineWorld
    {
        // Whole engine frames, so physics steps and writes transforms back.
        void Step(int count = 1)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        void Place(ECS::Entity entity, const HM::Vector3& position)
        {
            Ecs().GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
        }

        // A static box 100 x 100 x 1 m whose top is at z = 0.5.
        ECS::Entity Floor(int32_t layer = 0)
        {
            const ECS::Entity floor = Context.GetSceneManager().CreateGameObject();
            Ecs().GetComponent<TransformComponent>(floor).Scale = HM::Vector3(50.0f, 50.0f, 0.5f);
            ColliderComponent collider;
            collider.Layer = layer;
            Ecs().AddComponent(floor, collider);
            return floor;
        }

        // A scripted dynamic body without damping.
        ECS::Entity Body(const std::string& script, const HM::Vector3& position, float mass, float gravityScale)
        {
            const ECS::Entity body = AddScripted(script);
            Place(body, position);
            Ecs().AddComponent(body, ColliderComponent{});
            RigidBodyComponent rigid;
            rigid.Mass           = mass;
            rigid.GravityScale   = gravityScale;
            rigid.LinearDamping  = 0.0f;
            rigid.AngularDamping = 0.0f;
            Ecs().AddComponent(body, rigid);
            return body;
        }

        HM::Vector3 Position(ECS::Entity entity) { return Ecs().GetComponent<TransformComponent>(entity).Position; }
    };
}

TEST_CASE("Physics bindings - an impulse in OnStart sets the velocity a script reads back, and moves the body")
{
    PhysicsWorld world;
    world.WriteScript("Kick.lua", Script("Kick",
        "local body = self.entity:getRigidBody()\n"
        "body:addImpulse(Vector3(4, 0, 0))\n"
        "Log.info('velocity', body.velocity.x, body.velocity.z)\n"
        "Log.info('mass', body.mass, 'type', body.bodyType == RigidBodyType.Dynamic)"));
    const ECS::Entity body = world.Body("Scripts/Kick.lua", HM::Vector3(0.0f, 0.0f, 5.0f), 2.0f, 0.0f);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(60);
    CHECK(log.Lines("velocity 2.0 0.0").size() == 1);
    CHECK(log.Lines("mass 2.0 type true").size() == 1);
    CHECK(world.Position(body).x() == doctest::Approx(2.0f).epsilon(0.03));
    CHECK(world.Position(body).z() == doctest::Approx(5.0f));
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - a force added in every OnFixedUpdate accelerates the body by F / m")
{
    PhysicsWorld world;
    world.WriteScript("Thrust.lua", Script("Thrust", "self.body = self.entity:getRigidBody()",
        "self.body:addForce(Vector3(0, 0, 6))"));
    const ECS::Entity body = world.Body("Scripts/Thrust.lua", HM::Vector3(0.0f, 0.0f, 5.0f), 2.0f, 0.0f);

    REQUIRE(world.Context.Play());
    world.Step(60);
    // 60 steps of 6 N on 2 kg: 3 m/s, every step's force applied in its own step.
    CHECK(world.Context.GetPhysicsSystem()->GetLinearVelocity(body).z() == doctest::Approx(3.0f).epsilon(0.001));
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - Physics.raycast returns the entity below, and nil for a miss or a masked layer")
{
    PhysicsWorld      world;
    const ECS::Entity floor = world.Floor(3);
    world.WriteScript("Probe.lua", Script("Probe",
        "local hit = Physics.raycast(self.entity.transform.position, Vector3(0, 0, -1), 100)\n"
        "Log.info('hit', hit.entity.id, hit.point.z, hit.normal.z, hit.distance)\n"
        "Log.info('up', Physics.raycast(self.entity.transform.position, Vector3(0, 0, 1), 100))\n"
        "Log.info('short', Physics.raycast(self.entity.transform.position, Vector3(0, 0, -1), 5))\n"
        "Log.info('masked', Physics.raycast(self.entity.transform.position, Vector3(0, 0, -1), 100, 0xFFFF - 8))\n"
        "Log.info('layer', Physics.raycast(self.entity.transform.position, Vector3(0, 0, -1), 100, 8) ~= nil)"));
    const ECS::Entity probe = world.AddScripted("Scripts/Probe.lua");
    world.Place(probe, HM::Vector3(0.0f, 0.0f, 10.0f));

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("hit " + std::to_string(floor) + " 0.5 1.0 9.5").size() == 1);
    CHECK(log.Lines("up nil").size() == 1);
    CHECK(log.Lines("short nil").size() == 1);
    CHECK(log.Lines("masked nil").size() == 1);
    CHECK(log.Lines("layer true").size() == 1);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - mass and a collider's radius set during Play apply at once; Stop restores them")
{
    PhysicsWorld world;
    world.Floor();
    world.WriteScript("Grow.lua", Script("Grow",
        "local body = self.entity:getRigidBody()\n"
        "body.mass = 4\n"
        "body:addImpulse(Vector3(4, 0, 0))\n"
        "Log.info('heavier', body.velocity.x)\n"
        "body.velocity = Vector3(0, 0, 0)\n"
        "self.entity:getCollider().radius = 1",
        "", ""));
    const ECS::Entity ball = world.Body("Scripts/Grow.lua", HM::Vector3(0.0f, 0.0f, 3.0f), 1.0f, 1.0f);
    world.Ecs().GetComponent<ColliderComponent>(ball).Shape = ColliderShape::Sphere;

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(150);
    CHECK(log.Lines("heavier 1.0").size() == 1);
    // Resting on its new 1 m radius.
    CHECK(world.Position(ball).z() == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(world.Ecs().GetComponent<RigidBodyComponent>(ball).Mass == 4.0f);
    REQUIRE(world.Stop());
    CHECK(world.Ecs().GetComponent<RigidBodyComponent>(ball).Mass == 1.0f);
    CHECK(world.Ecs().GetComponent<ColliderComponent>(ball).Radius == 0.5f);
}

TEST_CASE("Physics bindings - gravity is the running world's, components come by key and are added by scripts")
{
    PhysicsWorld world;
    world.Floor();
    world.WriteScript("Float.lua", Script("Float",
        "Physics.gravity = Vector3(0, 0, 0)\n"
        "Log.info('gravity', Physics.gravity.z)\n"
        "local body = self.entity:getComponent('RigidBodyComponent')\n"
        "Log.info('by key', body.mass, self.entity:getComponent('ColliderComponent').shape == ColliderShape.Box)\n"
        "local other = Scene.spawn('Added')\n"
        "other:addCollider().shape = ColliderShape.Sphere\n"
        "other:addRigidBody()\n"
        "other.transform.position = Vector3(10, 0, 5)\n"
        "self.added = other",
        "", "self.frames = (self.frames or 0) + 1\n"
        "if self.frames == 30 then Log.info('added at', self.added.transform.position.z < 5) end"));
    const ECS::Entity body = world.Body("Scripts/Float.lua", HM::Vector3(0.0f, 0.0f, 5.0f), 1.0f, 1.0f);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(30);
    CHECK(log.Lines("gravity 0.0").size() == 1);
    CHECK(log.Lines("by key 1.0 true").size() == 1);
    // No gravity: the body stays, and so does the one the script added.
    CHECK(world.Position(body).z() == doctest::Approx(5.0f));
    CHECK(log.Lines("added at false").size() == 1);
    CHECK(world.Context.GetPhysicsSystem()->GetBodyCount() == 3);
    REQUIRE(world.Stop());

    // The next Play reads the settings again.
    REQUIRE(world.Context.Play());
    CHECK(world.Context.GetPhysicsSystem()->GetGravity().z() == doctest::Approx(-9.81f));
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - calls in Edit do nothing and warn once; bad arguments are script errors")
{
    PhysicsWorld world;
    world.WriteScript("Early.lua", "Physics.raycast(Vector3(0, 0, 0), Vector3(0, 0, -1))\nPhysics.gravity = Vector3(0, 0, 0)\n" +
                                       Script("Early", ""));
    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/Early.lua");
    (void)world.Scripts->DescribeScript("assets://Scripts/Early.lua");
    CHECK(log.Lines("[Script] Physics runs only in Play mode; Physics.raycast() did nothing.").size() == 1);
    CHECK(log.Lines("Physics runs only in Play mode").size() == 1);

    world.WriteScript("Bad.lua", Script("Bad",
        "local body = self.entity:getRigidBody()\n"
        "local collider = self.entity:getCollider()\n"
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('nan', function() body:addForce(Vector3(0 / 0, 0, 0)) end)\n"
        "try('number', function() body:addImpulse(5) end)\n"
        "try('type', function() body.bodyType = 7 end)\n"
        "try('shape', function() collider.shape = -1 end)\n"
        "try('layer', function() collider.layer = 16 end)\n"
        "try('mass', function() body.mass = 0 end)\n"
        "try('gravity', function() Physics.gravity = 3 end)\n"
        "try('ray', function() Physics.raycast(Vector3(0, 0, 0), 'down') end)"));
    (void)world.Body("Scripts/Bad.lua", HM::Vector3(0.0f, 0.0f, 5.0f), 1.0f, 0.0f);
    REQUIRE(world.Context.Play());
    world.Step();
    for (const char* label : { "nan false", "number false", "type false", "shape false", "layer false", "mass false",
                               "gravity false", "ray false" })
    {
        CAPTURE(label);
        CHECK(log.Lines(label).size() == 1);
    }
    CHECK(log.Lines("7 is not a RigidBodyType").size() == 1);
    CHECK(log.Lines("16 is not a physics layer").size() == 1);
    CHECK(log.Lines("must be a finite Vector3").size() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - shape casts and overlaps return the entities they find")
{
    PhysicsWorld world;
    // Static 2 m boxes: one along +x for the casts, three crates in a region and a trigger beside them.
    const auto Static = [&](const HM::Vector3& position, bool trigger = false)
    {
        const ECS::Entity entity = world.Context.GetSceneManager().CreateGameObject();
        world.Place(entity, position);
        ColliderComponent collider;
        collider.IsTrigger = trigger;
        world.Ecs().AddComponent(entity, collider);
        return entity;
    };
    const ECS::Entity box = Static(HM::Vector3(5.0f, 0.0f, 3.0f));
    std::string       crates;
    for (const float x : { 20.0f, 22.0f, 24.0f })
        crates += " " + std::to_string(Static(HM::Vector3(x, 0.0f, 3.0f)));
    const ECS::Entity trigger = Static(HM::Vector3(22.0f, 3.0f, 3.0f), true);
    (void)Static(HM::Vector3(40.0f, 0.0f, 3.0f));

    world.WriteScript("Probe.lua", Script("Probe",
        "local function ids(entities) local s = '' for _, e in ipairs(entities) do s = s .. ' ' .. e.id end return s end\n"
        "local origin, along = Vector3(0, 0, 3), Vector3(1, 0, 0)\n"
        "local hit = Physics.sphereCast(origin, 0.5, along, 100)\n"
        "Log.info('sphere', hit.entity.id, string.format('%.2f %.2f', hit.distance, hit.normal.x))\n"
        "hit = Physics.boxCast(origin, Vector3(0.5, 0.5, 0.5), Quat.fromEuler(0, 0, 90), along)\n"
        "Log.info('box', hit.entity.id, string.format('%.2f', hit.distance))\n"
        "hit = Physics.capsuleCast(origin, 0.25, 1.5, Vector3(0, 90, 0), along, 100)\n"
        "Log.info('capsule', hit.entity.id, string.format('%.2f', hit.distance))\n"
        "Log.info('missed', Physics.sphereCast(origin, 0.5, Vector3(0, 0, 1), 100))\n"
        "Log.info('masked', Physics.boxCast(origin, Vector3(1, 1, 1), nil, along, 100, 0xFFFE))\n"
        "Log.info('boxed' .. ids(Physics.overlapBox(Vector3(22, 0, 3), Vector3(4, 4, 1), nil)))\n"
        "Log.info('with triggers' .. ids(Physics.overlapBox(Vector3(22, 0, 3), Vector3(4, 4, 1), nil, 0xFFFF, true)))\n"
        "Log.info('capsule region' .. ids(Physics.overlapCapsule(Vector3(22, 0, 3), 0.5, 6, Vector3(0, 90, 0))))\n"
        "Log.info('empty', #Physics.overlapSphere(Vector3(30, 0, 3), 0.5))"));
    (void)world.AddScripted("Scripts/Probe.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    const std::string id = std::to_string(box);
    CHECK(log.Lines("sphere " + id + " 3.50 -1.00").size() == 1);
    CHECK(log.Lines("box " + id + " 3.50").size() == 1);
    CHECK(log.Lines("capsule " + id + " 3.25").size() == 1);    CHECK(log.Lines("missed nil").size() == 1);
    CHECK(log.Lines("masked nil").size() == 1);
    CHECK(log.Lines("boxed" + crates).size() == 1);
    CHECK(log.Lines("with triggers" + crates + " " + std::to_string(trigger)).size() == 1);
    CHECK(log.Lines("capsule region" + crates).size() == 1);
    CHECK(log.Lines("empty 0").size() == 1);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Physics bindings - shape queries in Edit warn once; bad arguments are script errors")
{
    PhysicsWorld world;
    world.WriteScript("Early.lua", "Physics.sphereCast(Vector3(0, 0, 0), 1, Vector3(0, 0, -1))\n"
                                   "Physics.overlapBox(Vector3(0, 0, 0), Vector3(1, 1, 1), nil)\n" +
                                       Script("Early", ""));
    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/Early.lua");
    CHECK(log.Lines("[Script] Physics runs only in Play mode; Physics.sphereCast() did nothing.").size() == 1);
    CHECK(log.Lines("Physics runs only in Play mode").size() == 1);

    world.WriteScript("Bad.lua", Script("Bad",
        "local o, d = Vector3(0, 0, 0), Vector3(1, 0, 0)\n"
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('nan', function() Physics.sphereCast(Vector3(0 / 0, 0, 0), 1, d) end)\n"
        "try('radius', function() Physics.sphereCast(o, 0, d) end)\n"
        "try('extent', function() Physics.boxCast(o, Vector3(1, -1, 1), nil, d) end)\n"
        "try('mask', function() Physics.overlapSphere(o, 1, 0x10000) end)\n"
        "try('rotation', function() Physics.boxCast(o, Vector3(1, 1, 1), 'up', d) end)\n"
        "try('height', function() Physics.overlapCapsule(o, 0.5, -1, nil) end)\n"
        "try('direction', function() Physics.capsuleCast(o, 0.5, 2, nil, 'x') end)\n"
        "try('distance', function() Physics.boxCast(o, Vector3(1, 1, 1), nil, d, 0 / 0) end)"));
    (void)world.AddScripted("Scripts/Bad.lua");
    REQUIRE(world.Context.Play());
    world.Step();
    for (const char* label : { "nan false", "radius false", "extent false", "mask false", "rotation false", "height false",
                               "direction false", "distance false" })
    {
        CAPTURE(label);
        CHECK(log.Lines(label).size() == 1);
    }
    CHECK(log.Lines("a sphere's radius must be above 0").size() == 1);
    CHECK(log.Lines("a box's half extents must be above 0").size() == 1);
    CHECK(log.Lines("a layer mask must be 0 to 65535").size() == 1);
    CHECK(log.Lines("a rotation must be a Quat or a Vector3 of Euler degrees").size() == 1);
    CHECK(log.Lines("a capsule's height must be 0 or more").size() == 1);
    REQUIRE(world.Stop());
}
