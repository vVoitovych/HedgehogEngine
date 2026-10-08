#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/PhysicsEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <optional>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    struct GameplayScene
    {
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();

        ECS::Entity Object(const HM::Vector3& position)
        {
            const ECS::Entity entity = Scenes.CreateGameObject();
            Ecs.GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            return entity;
        }

        // A static box 100 x 100 x 1 m whose top is at z = 0.5.
        ECS::Entity Floor()
        {
            const ECS::Entity floor = Object(HM::Vector3(0.0f, 0.0f, 0.0f));
            Ecs.GetComponent<TransformComponent>(floor).Scale = HM::Vector3(50.0f, 50.0f, 0.5f);
            Ecs.AddComponent(floor, ColliderComponent{});
            return floor;
        }

        ECS::Entity Body(const HM::Vector3& position, RigidBodyType type = RigidBodyType::Dynamic, float mass = 1.0f)
        {
            const ECS::Entity body = Object(position);
            Ecs.AddComponent(body, ColliderComponent{});
            RigidBodyComponent rigid;
            rigid.BodyType       = type;
            rigid.Mass           = mass;
            rigid.LinearDamping  = 0.0f;
            rigid.AngularDamping = 0.0f;
            Ecs.AddComponent(body, rigid);
            return body;
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem& Physics() { return *Context.GetPhysicsSystem(); }

        HM::Vector3 Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }

        void Move(ECS::Entity entity, const HM::Vector3& position)
        {
            Ecs.GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
        }
    };

    // Moves one entity along +X every fixed step, as gameplay would, before physics steps.
    struct Mover : ECS::System
    {
        Mover(ECS::Entity entity, float stepDistance) : Target(entity), Step(stepDistance) {}

        void OnFixedUpdate(ECS::ECS& ecs, float /*fixedDeltaTime*/) override
        {
            if (!ecs.IsAlive(Target))
                return;
            ecs.GetComponent<TransformComponent>(Target).Position += HM::Vector3(Step, 0.0f, 0.0f);
        }

        ECS::Entity Target;
        float       Step;
    };

    // Every physics event the bus carries, in order.
    struct EventLog
    {
        explicit EventLog(EventBus& bus)
        {
            bus.Subscribe<CollisionEnterEvent>([this](const CollisionEnterEvent& e) { CollisionEnters.push_back(e); });
            bus.Subscribe<CollisionExitEvent>([this](const CollisionExitEvent& e) { CollisionExits.push_back(e); });
            bus.Subscribe<TriggerEnterEvent>([this](const TriggerEnterEvent& e) { TriggerEnters.push_back(e); });
            bus.Subscribe<TriggerExitEvent>([this](const TriggerExitEvent& e) { TriggerExits.push_back(e); });
        }

        void Clear()
        {
            CollisionEnters.clear();
            CollisionExits.clear();
            TriggerEnters.clear();
            TriggerExits.clear();
        }

        size_t Count() const { return CollisionEnters.size() + CollisionExits.size() + TriggerEnters.size() + TriggerExits.size(); }

        std::vector<CollisionEnterEvent> CollisionEnters;
        std::vector<CollisionExitEvent>  CollisionExits;
        std::vector<TriggerEnterEvent>   TriggerEnters;
        std::vector<TriggerExitEvent>    TriggerExits;
    };
}

TEST_CASE("Physics gameplay - a kinematic paddle moved by gameplay follows its transform and pushes a box")
{
    GameplayScene scene;
    scene.Floor();
    const ECS::Entity box    = scene.Body(HM::Vector3(0.0f, 0.0f, 1.5f));
    const ECS::Entity paddle = scene.Body(HM::Vector3(-2.2f, 0.0f, 1.6f), RigidBodyType::Kinematic);
    scene.Ecs.RegisterSystem<Mover>(paddle, 0.05f);

    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    // The paddle's body is where gameplay put it at the end of the step.
    const HM::Vector3 target = scene.Position(paddle);
    CHECK(target.x() == doctest::Approx(-2.15f));
    scene.Frames(60);
    CHECK(scene.Position(paddle).x() == doctest::Approx(-2.2f + 0.05f * 61).epsilon(0.001));
    // 3 m on, the paddle has pushed the box ahead of it.
    CHECK(scene.Position(box).x() > 1.0f);
    CHECK(scene.Position(box).x() >= scene.Position(paddle).x() + 1.9f);
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics gameplay - gameplay writing a transform teleports a dynamic body and moves a static one")
{
    GameplayScene scene;
    scene.Floor();
    const ECS::Entity cube = scene.Body(HM::Vector3(0.0f, 0.0f, 5.0f));
    const ECS::Entity wall = scene.Object(HM::Vector3(0.0f, 10.0f, 3.0f));
    scene.Ecs.AddComponent(wall, ColliderComponent{});

    REQUIRE(scene.Context.Play());
    scene.Frames(10);
    scene.Move(cube, HM::Vector3(20.0f, 0.0f, 6.0f));
    scene.Frames(120);
    CHECK(scene.Position(cube).x() == doctest::Approx(20.0f).epsilon(0.001));
    CHECK(scene.Position(cube).z() == doctest::Approx(1.5f).epsilon(0.02));

    // The static wall, found by a ray, then moved by gameplay and found where it went.
    const HM::Vector3 down(0.0f, 0.0f, -1.0f);
    std::optional<PhysicsRayHit> hit = scene.Physics().Raycast(HM::Vector3(0.0f, 10.0f, 20.0f), down, 50.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Entity == wall);
    CHECK(hit->Point.z() == doctest::Approx(4.0f));
    scene.Move(wall, HM::Vector3(-30.0f, 10.0f, 3.0f));
    scene.Frames(1);
    hit = scene.Physics().Raycast(HM::Vector3(0.0f, 10.0f, 20.0f), down, 50.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Entity != wall);
    hit = scene.Physics().Raycast(HM::Vector3(-30.0f, 10.0f, 20.0f), down, 50.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Entity == wall);
    CHECK(hit->Normal.z() == doctest::Approx(1.0f));
    CHECK(hit->Distance == doctest::Approx(16.0f));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics gameplay - collision events name both entities, and trigger events their trigger")
{
    GameplayScene     scene;
    EventLog          log(scene.Context.GetEventBus());
    const ECS::Entity floor = scene.Floor();
    const ECS::Entity ball  = scene.Body(HM::Vector3(0.0f, 0.0f, 3.0f));
    scene.Ecs.GetComponent<ColliderComponent>(ball).Shape = ColliderShape::Sphere;

    REQUIRE(scene.Context.Play());
    scene.Frames(120);
    REQUIRE(log.CollisionEnters.size() == 1);
    CHECK(log.Count() == 1);
    const CollisionEnterEvent& enter = log.CollisionEnters[0];
    CHECK(((enter.EntityA == floor && enter.EntityB == ball) || (enter.EntityA == ball && enter.EntityB == floor)));
    CHECK(enter.Point.z() == doctest::Approx(0.5f).epsilon(0.05));

    log.Clear();
    scene.Scenes.DeleteGameObject(ball);
    scene.Frames(3);
    REQUIRE(log.CollisionExits.size() == 1);
    CHECK(log.Count() == 1);
    const CollisionExitEvent& exit = log.CollisionExits[0];
    CHECK(((exit.EntityA == floor && exit.EntityB == ball) || (exit.EntityA == ball && exit.EntityB == floor)));

    // A ball dropped through a trigger box.
    log.Clear();
    const ECS::Entity trigger = scene.Object(HM::Vector3(10.0f, 0.0f, 5.0f));
    ColliderComponent zone;
    zone.IsTrigger = true;
    scene.Ecs.AddComponent(trigger, zone);
    const ECS::Entity faller = scene.Body(HM::Vector3(10.0f, 0.0f, 10.0f));
    scene.Ecs.GetComponent<ColliderComponent>(faller).Shape = ColliderShape::Sphere;
    scene.Frames(150);
    REQUIRE(log.TriggerEnters.size() == 1);
    REQUIRE(log.TriggerExits.size() == 1);
    CHECK(log.TriggerEnters[0].Trigger == trigger);
    CHECK(log.TriggerEnters[0].Other == faller);
    CHECK(log.TriggerExits[0].Trigger == trigger);
    CHECK(log.TriggerExits[0].Other == faller);
    CHECK(log.CollisionEnters.size() == 1); // it lands on the floor

    // Stop publishes no Exit for what still touches.
    log.Clear();
    REQUIRE(scene.Context.Stop());
    scene.Frames(5);
    CHECK(log.Count() == 0);
}

TEST_CASE("Physics gameplay - velocities and impulses act through entities")
{
    GameplayScene     scene;
    const ECS::Entity body = scene.Body(HM::Vector3(0.0f, 0.0f, 5.0f), RigidBodyType::Dynamic, 2.0f);
    scene.Ecs.GetComponent<RigidBodyComponent>(body).GravityScale = 0.0f;
    const ECS::Entity none = scene.Object(HM::Vector3(0.0f, 0.0f, 0.0f));

    // Nothing before Play, or for an entity without a body.
    scene.Physics().SetLinearVelocity(body, HM::Vector3(1.0f, 0.0f, 0.0f));
    CHECK(scene.Physics().GetLinearVelocity(body).x() == 0.0f);

    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    scene.Physics().AddImpulse(body, HM::Vector3(4.0f, 0.0f, 0.0f));
    CHECK(scene.Physics().GetLinearVelocity(body).x() == doctest::Approx(2.0f));
    scene.Physics().SetLinearVelocity(body, HM::Vector3(0.0f, 3.0f, 0.0f));
    CHECK(scene.Physics().GetLinearVelocity(body).y() == doctest::Approx(3.0f));
    scene.Physics().SetAngularVelocity(body, HM::Vector3(0.0f, 0.0f, 1.0f));
    CHECK(scene.Physics().GetAngularVelocity(body).z() == doctest::Approx(1.0f));
    scene.Physics().AddAngularImpulse(body, HM::Vector3(0.0f, 0.0f, 1.0f));
    CHECK(scene.Physics().GetAngularVelocity(body).z() > 1.0f);

    // A force for one step: F dt / m.
    scene.Physics().SetLinearVelocity(body, HM::Vector3(0.0f, 0.0f, 0.0f));
    scene.Physics().SetAngularVelocity(body, HM::Vector3(0.0f, 0.0f, 0.0f));
    scene.Physics().AddForce(body, HM::Vector3(0.0f, 0.0f, 12.0f));
    scene.Physics().AddTorque(body, HM::Vector3(0.0f, 0.0f, 1.0f));
    scene.Frames(1);
    CHECK(scene.Physics().GetLinearVelocity(body).z() == doctest::Approx(12.0f * FRAME / 2.0f).epsilon(0.001));
    CHECK(scene.Physics().GetAngularVelocity(body).z() > 0.0f);

    scene.Physics().AddImpulse(none, HM::Vector3(1.0f, 0.0f, 0.0f));
    CHECK(scene.Physics().GetLinearVelocity(none).x() == 0.0f);
    REQUIRE(scene.Context.Stop());
    CHECK(scene.Physics().GetLinearVelocity(body).z() == 0.0f);
}

TEST_CASE("Physics gameplay - RebuildBody applies an edited collider and keeps the velocity")
{
    GameplayScene scene;
    scene.Floor();
    const ECS::Entity cube = scene.Body(HM::Vector3(0.0f, 0.0f, 8.0f));
    REQUIRE(scene.Context.Play());
    scene.Frames(10);
    const HM::Vector3 velocity = scene.Physics().GetLinearVelocity(cube);
    CHECK(velocity.z() < -1.0f);

    scene.Ecs.GetComponent<ColliderComponent>(cube).Size = HM::Vector3(4.0f, 4.0f, 4.0f);
    scene.Physics().RebuildBody(scene.Ecs, cube);
    CHECK(scene.Physics().GetLinearVelocity(cube).z() == doctest::Approx(velocity.z()));
    CHECK(scene.Physics().GetBodyCount() == 2);

    scene.Frames(150);
    // Resting on its new 2 m half height.
    CHECK(scene.Position(cube).z() == doctest::Approx(2.5f).epsilon(0.02));
    REQUIRE(scene.Context.Stop());
}
