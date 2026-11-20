#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/PhysicsEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <optional>
#include <string>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;
    // The floor's top, the default radius and half its default height.
    constexpr float REST_HEIGHT = 0.5f + 0.9f;

    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Text += line + '\n'; }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        [[nodiscard]] size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = m_Text.find(fragment); at != std::string::npos; at = m_Text.find(fragment, at + 1))
                ++count;
            return count;
        }

    private:
        std::string m_Text;
        int         m_Sink = 0;
    };

    struct CharacterScene
    {
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();

        ECS::Entity Object(const HM::Vector3& position, std::optional<ECS::Entity> parent = std::nullopt)
        {
            const ECS::Entity entity = Scenes.CreateGameObject(parent);
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

        ECS::Entity Character(const HM::Vector3& position, std::optional<ECS::Entity> parent = std::nullopt)
        {
            const ECS::Entity entity = Object(position, parent);
            Ecs.AddComponent(entity, CharacterControllerComponent{});
            return entity;
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem& Physics() { return *Context.GetPhysicsSystem(); }
        HM::Vector3    Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }
    };
}

TEST_CASE("Character controller - it round-trips through a snapshot, its runtime fields never written")
{
    CharacterScene    scene;
    const ECS::Entity entity     = scene.Character(HM::Vector3(1.0f, 2.0f, 3.0f));
    auto&             controller = scene.Ecs.GetComponent<CharacterControllerComponent>(entity);
    controller.Radius          = 0.35f;
    controller.Height          = 1.6f;
    controller.StepHeight      = 0.25f;
    controller.Layer           = 3;
    controller.GravityScale    = 2.0f;
    controller.DesiredVelocity = HM::Vector3(9.0f, 9.0f, 9.0f);
    controller.PendingJump     = 7.0f;

    const SceneSnapshot snapshot = scene.Scenes.CaptureSnapshot();
    CHECK(snapshot.Yaml.find("CharacterControllerComponent") != std::string::npos);
    CHECK(snapshot.Yaml.find("StepHeight: 0.25") != std::string::npos);
    CHECK(snapshot.Yaml.find("DesiredVelocity") == std::string::npos);
    CHECK(snapshot.Yaml.find("PendingJump") == std::string::npos);
    REQUIRE(scene.Scenes.RestoreSnapshot(snapshot));
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
    const auto& restored = scene.Ecs.GetComponent<CharacterControllerComponent>(entity);
    CHECK(restored.Layer == 3);
    CHECK(restored.GravityScale == 2.0f);
    CHECK(restored.PendingJump == 0.0f);
}

TEST_CASE("Character controller - it lands and is grounded in Play; Edit and Pause hold it; Stop restores")
{
    CharacterScene scene;
    scene.Floor();
    const ECS::Entity character = scene.Character(HM::Vector3(0.0f, 0.0f, 3.0f));
    const std::string before    = scene.Scenes.CaptureSnapshot().Yaml;

    scene.Frames(30);
    CHECK(scene.Position(character).z() == 3.0f);
    CHECK(scene.Physics().GetCharacterCount() == 0);

    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    CHECK(scene.Physics().GetCharacterCount() == 1);
    CHECK_FALSE(scene.Physics().IsCharacterGrounded(character));
    scene.Frames(90);
    CHECK(scene.Physics().IsCharacterGrounded(character));
    CHECK(scene.Position(character).z() == doctest::Approx(REST_HEIGHT).epsilon(0.01));
    CHECK(scene.Physics().GetCharacterGroundNormal(character).z() == doctest::Approx(1.0f).epsilon(1e-3));

    scene.Physics().SetCharacterMove(character, HM::Vector3(2.0f, 0.0f, 0.0f));
    REQUIRE(scene.Context.Pause());
    const HM::Vector3 paused = scene.Position(character);
    scene.Frames(30);
    CHECK(scene.Position(character) == paused);
    REQUIRE(scene.Context.Resume());

    REQUIRE(scene.Context.Stop());
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == before);
    CHECK(scene.Physics().GetCharacterCount() == 0);
}

TEST_CASE("Character controller - it walks 2 m in a second, and a jump rises and lands again")
{
    CharacterScene scene;
    scene.Floor();
    const ECS::Entity character = scene.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT + 0.05f));
    REQUIRE(scene.Context.Play());
    scene.Frames(30);
    REQUIRE(scene.Physics().IsCharacterGrounded(character));

    const float start = scene.Position(character).x();
    scene.Physics().SetCharacterMove(character, HM::Vector3(2.0f, 0.0f, 0.0f));
    scene.Frames(60);
    CHECK(scene.Position(character).x() - start == doctest::Approx(2.0f).epsilon(0.02));
    CHECK(scene.Physics().GetCharacterVelocity(character).x() == doctest::Approx(2.0f).epsilon(0.02));

    scene.Physics().SetCharacterMove(character, HM::Vector3(0.0f, 0.0f, 0.0f));
    scene.Physics().CharacterJump(character, 5.0f);
    scene.Frames(1);
    float peak     = scene.Position(character).z();
    bool  airborne = true;
    for (int frame = 0; frame < 50; ++frame)
    {
        airborne = airborne && !scene.Physics().IsCharacterGrounded(character);
        peak     = std::max(peak, scene.Position(character).z());
        scene.Frames(1);
    }
    CHECK(airborne); // 5 m/s up is about a second in the air
    CHECK(peak > REST_HEIGHT + 1.0f);
    scene.Frames(60);
    CHECK(scene.Physics().IsCharacterGrounded(character));
    CHECK(scene.Position(character).z() == doctest::Approx(REST_HEIGHT).epsilon(0.01));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Character controller - under a moved parent its local position stays consistent with the parent")
{
    CharacterScene scene;
    scene.Floor();
    const ECS::Entity parent    = scene.Object(HM::Vector3(10.0f, 4.0f, 0.0f));
    const ECS::Entity character = scene.Character(HM::Vector3(0.0f, 0.0f, 3.0f), parent);
    REQUIRE(scene.Context.Play());
    scene.Frames(90);
    REQUIRE(scene.Physics().IsCharacterGrounded(character));
    const HM::Vector3 local = scene.Position(character);
    CHECK(local.x() == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(local.y() == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(local.z() == doctest::Approx(REST_HEIGHT).epsilon(0.01));
    const HM::Vector4& world = scene.Ecs.GetComponent<TransformComponent>(character).ObjMatrix[3];
    CHECK(world.x() == doctest::Approx(10.0f).epsilon(0.01));
    CHECK(world.y() == doctest::Approx(4.0f).epsilon(0.01));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Character controller - it pushes a crate, and a trigger it walks through names it")
{
    CharacterScene scene;
    scene.Floor();
    const ECS::Entity character = scene.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT + 0.05f));
    const ECS::Entity crate     = scene.Object(HM::Vector3(1.5f, 0.0f, 0.75f));
    scene.Ecs.GetComponent<TransformComponent>(crate).Scale = HM::Vector3(0.25f, 0.25f, 0.25f);
    scene.Ecs.AddComponent(crate, ColliderComponent{});
    scene.Ecs.AddComponent(crate, RigidBodyComponent{});
    const ECS::Entity zone = scene.Object(HM::Vector3(6.0f, 0.0f, 1.5f)); // a 2 m cube across its path
    ColliderComponent trigger;
    trigger.IsTrigger = true;
    scene.Ecs.AddComponent(zone, trigger);

    int enters = 0;
    int exits  = 0;
    scene.Context.GetEventBus().Subscribe<TriggerEnterEvent>([&](const TriggerEnterEvent& event)
    {
        if (event.Trigger == zone && event.Other == character)
            ++enters;
    });
    scene.Context.GetEventBus().Subscribe<TriggerExitEvent>([&](const TriggerExitEvent& event)
    {
        if (event.Trigger == zone && event.Other == character)
            ++exits;
    });

    REQUIRE(scene.Context.Play());
    scene.Frames(20);
    scene.Physics().SetCharacterMove(character, HM::Vector3(2.0f, 0.0f, 0.0f));
    scene.Frames(90);
    CHECK(scene.Position(crate).x() > 2.0f);

    // On through the trigger and out the far side.
    scene.Frames(240);
    CHECK(scene.Position(character).x() > 7.5f);
    CHECK(enters == 1);
    CHECK(exits == 1);
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Character controller - a collider beside it warns once; removing it or its entity destroys it")
{
    CharacterScene scene;
    scene.Floor();
    const ECS::Entity both = scene.Character(HM::Vector3(0.0f, 0.0f, 3.0f));
    scene.Ecs.AddComponent(both, ColliderComponent{});
    const ECS::Entity other = scene.Character(HM::Vector3(5.0f, 0.0f, 3.0f));

    LogCapture log;
    REQUIRE(scene.Context.Play());
    scene.Frames(10);
    CHECK(log.Count("has a character controller and a collider") == 1);
    CHECK(scene.Physics().GetCharacterCount() == 2);
    CHECK_FALSE(scene.Physics().GetBody(both).IsSet()); // the floor alone has a body
    CHECK(scene.Physics().GetBodyCount() == 1);

    scene.Ecs.RemoveComponent<CharacterControllerComponent>(both);
    scene.Frames(1);
    CHECK(scene.Physics().GetCharacterCount() == 1);
    CHECK(scene.Physics().GetBodyCount() == 2); // its collider now makes a body
    scene.Scenes.DeleteGameObject(other);
    scene.Frames(1);
    CHECK(scene.Physics().GetCharacterCount() == 0);
    REQUIRE(scene.Context.Stop());
}
