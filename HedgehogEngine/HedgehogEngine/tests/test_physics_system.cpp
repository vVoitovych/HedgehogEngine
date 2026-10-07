#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <cmath>
#include <optional>
#include <string>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

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

        size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = m_Text.find(fragment); at != std::string::npos; at = m_Text.find(fragment, at + 1))
                ++count;
            return count;
        }

    private:
        std::string m_Text;
        int         m_Sink;
    };

    struct PhysicsScene
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

        // A dynamic 2 m cube (the default box collider).
        ECS::Entity Cube(const HM::Vector3& position, std::optional<ECS::Entity> parent = std::nullopt)
        {
            const ECS::Entity cube = Object(position, parent);
            Ecs.AddComponent(cube, ColliderComponent{});
            Ecs.AddComponent(cube, RigidBodyComponent{});
            return cube;
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem& Physics() { return *Context.GetPhysicsSystem(); }

        HM::Vector3 Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }

        HM::Vector3 WorldPosition(ECS::Entity entity)
        {
            const HM::Vector4& column = Ecs.GetComponent<TransformComponent>(entity).ObjMatrix[3];
            return HM::Vector3(column.x(), column.y(), column.z());
        }
    };
}

TEST_CASE("Physics system - a dynamic cube falls onto a static floor in Play only, and Stop restores the scene")
{
    PhysicsScene scene;
    scene.Floor();
    const ECS::Entity cube = scene.Cube(HM::Vector3(0.0f, 0.0f, 5.0f));
    scene.Frames(1);
    const std::string before = scene.Scenes.CaptureSnapshot().Yaml;

    // Edit mode simulates nothing and starts no world.
    scene.Frames(30);
    CHECK(scene.Position(cube).z() == 5.0f);
    CHECK_FALSE(scene.Physics().IsWorldStarted());

    REQUIRE(scene.Context.Play());
    CHECK(scene.Physics().IsWorldStarted());
    CHECK(scene.Physics().GetBodyCount() == 2);
    CHECK(scene.Ecs.GetComponent<RigidBodyComponent>(cube).Body.IsSet());
    scene.Frames(120);
    // Resting: its 1 m half height on the floor's top.
    CHECK(scene.Position(cube).z() == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(scene.WorldPosition(cube).z() == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(scene.Position(cube).x() == doctest::Approx(0.0f).epsilon(0.01));

    // Paused holds.
    REQUIRE(scene.Context.Pause());
    const float held = scene.Position(cube).z();
    scene.Frames(30);
    CHECK(scene.Position(cube).z() == held);
    REQUIRE(scene.Context.Resume());

    REQUIRE(scene.Context.Stop());
    CHECK(scene.Physics().GetBodyCount() == 0);
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == before);
    CHECK(scene.Position(cube).z() == 5.0f);

    // The world stays started, and a second Play falls the same way.
    REQUIRE(scene.Context.Play());
    scene.Frames(120);
    CHECK(scene.Position(cube).z() == doctest::Approx(1.5f).epsilon(0.02));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics system - colliders added and deleted during Play gain and lose their bodies")
{
    PhysicsScene scene;
    scene.Floor();
    scene.Cube(HM::Vector3(0.0f, 0.0f, 3.0f));
    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    CHECK(scene.Physics().GetBodyCount() == 2);

    const ECS::Entity added = scene.Cube(HM::Vector3(5.0f, 0.0f, 3.0f));
    CHECK(scene.Physics().GetBodyCount() == 2);
    scene.Frames(1);
    CHECK(scene.Physics().GetBodyCount() == 3);
    CHECK(scene.Physics().GetBody(added).IsSet());

    scene.Scenes.DeleteGameObject(added);
    scene.Frames(1);
    CHECK(scene.Physics().GetBodyCount() == 2);

    // A collider removed from a live entity loses its body too.
    const ECS::Entity stripped = scene.Cube(HM::Vector3(-5.0f, 0.0f, 3.0f));
    scene.Frames(1);
    CHECK(scene.Physics().GetBodyCount() == 3);
    scene.Ecs.RemoveComponent<ColliderComponent>(stripped);
    scene.Frames(1);
    CHECK(scene.Physics().GetBodyCount() == 2);
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics system - a save loaded during Play rebuilds the bodies where the save left them")
{
    TempDir      saves;
    PhysicsScene scene;
    scene.Context.GetSaveGames().SetSaveDirectory(saves.Path());
    scene.Floor();
    const ECS::Entity cube = scene.Cube(HM::Vector3(0.0f, 0.0f, 8.0f));
    REQUIRE(scene.Context.Play());
    scene.Frames(10);
    const float saved = scene.Position(cube).z();
    CHECK(saved < 8.0f);
    REQUIRE(scene.Context.GetSaveGames().RequestSave("physics"));
    scene.Frames(1);
    const float afterSave = scene.Position(cube).z();

    scene.Frames(120);
    CHECK(scene.Position(cube).z() == doctest::Approx(1.5f).epsilon(0.02));
    REQUIRE(scene.Context.GetSaveGames().RequestLoad("physics"));
    scene.Frames(1);
    // The world is the save's again, and its bodies were made anew from it: the cube falls again.
    CHECK(scene.Physics().GetBodyCount() == 2);
    CHECK(scene.Position(cube).z() > 3.0f);
    CHECK(scene.Position(cube).z() <= afterSave);
    scene.Frames(120);
    CHECK(scene.Position(cube).z() == doctest::Approx(1.5f).epsilon(0.02));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics system - a body under a moved and turned static parent lands where its top-level twin does")
{
    PhysicsScene scene;
    scene.Floor();
    // The parent is at (10, 0, 2) turned 90 degrees about Z, so local (0, 3, 3) is world (7, 0, 5).
    const ECS::Entity parent = scene.Object(HM::Vector3(10.0f, 0.0f, 2.0f));
    scene.Ecs.GetComponent<TransformComponent>(parent).Rotation = HM::Vector3(0.0f, 0.0f, 90.0f);
    const ECS::Entity child = scene.Cube(HM::Vector3(0.0f, 3.0f, 3.0f), parent);
    const ECS::Entity twin  = scene.Cube(HM::Vector3(7.0f, 20.0f, 5.0f));
    scene.Frames(1);
    CHECK(scene.WorldPosition(child).x() == doctest::Approx(7.0f).epsilon(0.001));
    CHECK(scene.WorldPosition(child).z() == doctest::Approx(5.0f).epsilon(0.001));

    REQUIRE(scene.Context.Play());
    scene.Frames(120);
    const HM::Vector3 childWorld = scene.WorldPosition(child);
    const HM::Vector3 twinWorld  = scene.WorldPosition(twin);
    CHECK(childWorld.x() == doctest::Approx(twinWorld.x()).epsilon(0.01));
    CHECK(childWorld.y() == doctest::Approx(twinWorld.y() - 20.0f).epsilon(0.01));
    CHECK(childWorld.z() == doctest::Approx(twinWorld.z()).epsilon(0.01));
    CHECK(childWorld.z() == doctest::Approx(1.5f).epsilon(0.02));

    // Its local transform says the same through the parent: turned back by the parent's turn.
    const HM::Vector3 local = scene.Position(child);
    CHECK(local.x() == doctest::Approx(0.0f).epsilon(0.01));
    CHECK(local.y() == doctest::Approx(3.0f).epsilon(0.01));
    CHECK(local.z() == doctest::Approx(-0.5f).epsilon(0.02));
    // It keeps the parent's turn it started with: no local rotation, its world X axis along +Y.
    const HM::Vector3 rotation = scene.Ecs.GetComponent<TransformComponent>(child).Rotation;
    CHECK(std::abs(rotation.x()) < 0.5f);
    CHECK(std::abs(rotation.y()) < 0.5f);
    CHECK(std::abs(rotation.z()) < 0.5f);
    const HM::Vector4& childAxis = scene.Ecs.GetComponent<TransformComponent>(child).ObjMatrix[0];
    CHECK(std::abs(childAxis.x()) < 0.01f);
    CHECK(childAxis.y() == doctest::Approx(1.0f).epsilon(0.01));
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics system - no collider never starts the world, and a rigid body without one warns once")
{
    PhysicsScene      scene;
    const ECS::Entity lonely = scene.Object(HM::Vector3(0.0f, 0.0f, 3.0f));
    scene.Ecs.AddComponent(lonely, RigidBodyComponent{});

    LogCapture log;
    REQUIRE(scene.Context.Play());
    scene.Frames(60);
    CHECK_FALSE(scene.Physics().IsWorldStarted());
    CHECK(scene.Physics().GetBodyCount() == 0);
    CHECK(scene.Position(lonely).z() == 3.0f);
    CHECK(log.Count("has a rigid body but no collider") == 1);
    REQUIRE(scene.Context.Stop());

    // A collider added during Play starts the world at the next fixed step.
    REQUIRE(scene.Context.Play());
    scene.Ecs.AddComponent(lonely, ColliderComponent{});
    scene.Frames(30);
    CHECK(scene.Physics().IsWorldStarted());
    CHECK(scene.Position(lonely).z() < 3.0f);
    REQUIRE(scene.Context.Stop());
}

TEST_CASE("Physics system - a static rigid body and a collider alone are static bodies")
{
    PhysicsScene      scene;
    const ECS::Entity wall = scene.Object(HM::Vector3(0.0f, 0.0f, 10.0f));
    scene.Ecs.AddComponent(wall, ColliderComponent{});
    RigidBodyComponent body;
    body.BodyType           = RigidBodyType::Static;
    const ECS::Entity pillar = scene.Object(HM::Vector3(5.0f, 0.0f, 10.0f));
    scene.Ecs.AddComponent(pillar, ColliderComponent{});
    scene.Ecs.AddComponent(pillar, body);

    REQUIRE(scene.Context.Play());
    scene.Frames(60);
    CHECK(scene.Physics().GetBodyCount() == 2);
    CHECK(scene.Position(wall).z() == 10.0f);
    CHECK(scene.Position(pillar).z() == 10.0f);
    REQUIRE(scene.Context.Stop());
}
