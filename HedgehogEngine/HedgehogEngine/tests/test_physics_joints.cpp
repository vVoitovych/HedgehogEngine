#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/JointComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    // Every warning and error the Logger writes while it lives.
    class Problems
    {
    public:
        Problems()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel level, const std::string& line) {
                  if (level == EngineLogger::LogLevel::Error)
                      Errors += line + '\n';
                  else if (level == EngineLogger::LogLevel::Warning)
                      Warnings += line + '\n';
              }))
        {
        }
        ~Problems() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        Problems(const Problems&)            = delete;
        Problems& operator=(const Problems&) = delete;

        std::string Errors;
        std::string Warnings;

    private:
        int m_Sink;
    };

    struct JointScene
    {
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();

        // A 2 m box scaled by scale (its collider's default size fits the default cube), static without
        // a mass.
        ECS::Entity Box(const HM::Vector3& position, const HM::Vector3& scale, float mass = 0.0f,
                        ECS::Entity parent = ECS::INVALID_ENTITY)
        {
            const ECS::Entity entity = parent == ECS::INVALID_ENTITY ? Scenes.CreateGameObject()
                                                                     : Scenes.CreateGameObject(parent);
            auto& transform    = Ecs.GetComponent<TransformComponent>(entity);
            transform.Position = position;
            transform.Scale    = scale;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            Ecs.AddComponent(entity, ColliderComponent{});
            if (mass > 0.0f)
            {
                RigidBodyComponent rigid;
                rigid.Mass = mass;
                Ecs.AddComponent(entity, rigid);
            }
            return entity;
        }

        JointComponent& Join(ECS::Entity entity, JointType type, ECS::Entity connected = ECS::INVALID_ENTITY)
        {
            JointComponent joint;
            joint.Type            = type;
            joint.ConnectedEntity = connected;
            Ecs.AddComponent(entity, joint);
            return Ecs.GetComponent<JointComponent>(entity);
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem&   Physics() { return *Context.GetPhysicsSystem(); }
        HP::PhysicsWorld& World() { return Context.GetPhysicsWorld(); }
        HM::Vector3      Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }
    };

    // A door 1.8 m wide hinged about +Z at its left edge to a static post at the origin.
    struct Door
    {
        ECS::Entity Post;
        ECS::Entity Panel;

        Door(JointScene& scene, bool limited)
        {
            Post  = scene.Box(HM::Vector3(0.0f, 0.0f, 2.0f), HM::Vector3(0.1f, 0.1f, 1.0f));
            Panel = scene.Box(HM::Vector3(1.05f, 0.0f, 2.0f), HM::Vector3(0.9f, 0.04f, 1.0f), 1.0f);
            scene.Ecs.GetComponent<RigidBodyComponent>(Panel).GravityScale = 0.0f;
            JointComponent& hinge = scene.Join(Panel, JointType::Hinge, Post);
            hinge.Anchor          = HM::Vector3(-1.0f, 0.0f, 0.0f); // its left edge, in its scaled space
            hinge.Axis            = HM::Vector3(0.0f, 0.0f, 1.0f);
            hinge.LimitsEnabled   = limited;
            hinge.MinLimit        = -60.0f;
            hinge.MaxLimit        = 60.0f;
        }
    };
}

TEST_CASE("Joints - the component round-trips through a snapshot, its constraint never written")
{
    JointScene        scene;
    const ECS::Entity post  = scene.Box(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    const ECS::Entity panel = scene.Box(HM::Vector3(3.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f), 1.0f);
    JointComponent&   joint = scene.Join(panel, JointType::Slider, post);
    joint.Anchor            = HM::Vector3(1.0f, 2.0f, 3.0f);
    joint.LimitsEnabled     = true;
    joint.MinLimit          = -0.5f;
    joint.MaxLimit          = 1.5f;
    joint.SpringFrequency   = 3.0f;
    joint.Constraint        = HP::ConstraintHandle{ 4, 2 };

    const SceneSnapshot snapshot = scene.Scenes.CaptureSnapshot();
    CHECK(snapshot.Yaml.find("JointComponent") != std::string::npos);
    CHECK(snapshot.Yaml.find("ConnectedEntity: " + std::to_string(post)) != std::string::npos);
    CHECK(snapshot.Yaml.find("Constraint") == std::string::npos);
    REQUIRE(scene.Scenes.RestoreSnapshot(snapshot));
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
    const JointComponent& restored = scene.Ecs.GetComponent<JointComponent>(panel);
    CHECK(restored.Type == JointType::Slider);
    CHECK(restored.ConnectedEntity == post);
    CHECK(restored.MaxLimit == 1.5f);
    CHECK(restored.SpringFrequency == 3.0f);
    CHECK_FALSE(restored.Constraint.IsSet());
}

TEST_CASE("Joints - a prefab's joint to an entity inside it names that entity's copy")
{
    TempDir    dir;
    JointScene scene;
    auto       fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("prefabs://", dir.Path());
    REQUIRE(scene.Context.GetFileSystem().Register(std::move(fs)));

    const ECS::Entity post  = scene.Box(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    const ECS::Entity panel = scene.Box(HM::Vector3(3.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f), 1.0f, post);
    scene.Join(panel, JointType::Hinge, post);
    REQUIRE(scene.Context.GetPrefabs().CreatePrefab(post, "prefabs://Door.prefab"));

    const ECS::Entity copy = scene.Context.GetPrefabs().Instantiate("prefabs://Door.prefab");
    REQUIRE(copy != ECS::INVALID_ENTITY);
    const std::vector<ECS::Entity>& children = scene.Ecs.GetComponent<ECS::HierarchyComponent>(copy).Children;
    REQUIRE(children.size() == 1);
    CHECK(children[0] != panel);
    CHECK(scene.Ecs.GetComponent<JointComponent>(children[0]).ConnectedEntity == copy);
}

TEST_CASE("Joints - a door hinged to a static post swings when pushed and stops at its limit")
{
    JointScene scene;
    const Door door(scene, true);
    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    const HP::ConstraintHandle hinge = scene.Physics().GetJoint(door.Panel);
    REQUIRE(scene.World().IsValid(hinge));
    CHECK(scene.Physics().GetJointCount() == 1);
    CHECK(scene.Ecs.GetComponent<JointComponent>(door.Panel).Constraint == hinge);

    scene.Physics().AddImpulse(door.Panel, HM::Vector3(0.0f, 1.5f, 0.0f));
    float widest = 0.0f;
    for (int frame = 0; frame < 120; ++frame)
    {
        scene.Frames(1);
        widest = std::max(widest, std::abs(scene.World().GetConstraintAngle(hinge)));
        // It turns about its hinge, which stays put.
        CHECK(std::abs(scene.Position(door.Panel).z() - 2.0f) < 0.01f);
    }
    // It reaches its limit and goes no further (rebounding a little off it).
    CHECK(widest > 58.0f);
    CHECK(widest < 62.0f);
    CHECK(std::abs(scene.World().GetConstraintAngle(hinge)) > 45.0f);
    REQUIRE(scene.Context.Stop());
    CHECK(scene.Physics().GetJointCount() == 0);
}

TEST_CASE("Joints - a chain of three boxes on distance joints hangs from a world anchor")
{
    JointScene  scene;
    ECS::Entity links[3];
    for (int index = 0; index < 3; ++index)
    {
        links[index] = scene.Box(HM::Vector3(0.0f, 0.0f, 9.5f - static_cast<float>(index)), HM::Vector3(0.2f, 0.2f, 0.2f), 1.0f);
        JointComponent& joint = scene.Join(links[index], JointType::Distance, index == 0 ? ECS::INVALID_ENTITY : links[index - 1]);
        joint.Anchor          = HM::Vector3(0.0f, 0.0f, 1.0f); // its top face
        // The world point above it, or the bottom face of the link above.
        joint.ConnectedAnchor = index == 0 ? HM::Vector3(0.0f, 0.0f, 10.0f) : HM::Vector3(0.0f, 0.0f, -1.0f);
    }
    const std::string before = scene.Scenes.CaptureSnapshot().Yaml;

    REQUIRE(scene.Context.Play());
    scene.Frames(120);
    CHECK(scene.Physics().GetJointCount() == 3);
    for (int index = 0; index < 3; ++index)
    {
        CAPTURE(index);
        CHECK(scene.Position(links[index]).z() == doctest::Approx(9.5f - static_cast<float>(index)).epsilon(0.005));
        CHECK(std::abs(scene.Position(links[index]).x()) < 0.01f);
    }
    REQUIRE(scene.Context.Stop());
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == before);
}

TEST_CASE("Joints - deleting the connected entity removes the joint quietly; one added in Play is made")
{
    JointScene        scene;
    const ECS::Entity top    = scene.Box(HM::Vector3(0.0f, 0.0f, 9.5f), HM::Vector3(0.2f, 0.2f, 0.2f), 1.0f);
    const ECS::Entity bottom = scene.Box(HM::Vector3(0.0f, 0.0f, 8.5f), HM::Vector3(0.2f, 0.2f, 0.2f), 1.0f);
    JointComponent&   upper  = scene.Join(top, JointType::Distance);
    upper.Anchor             = HM::Vector3(0.0f, 0.0f, 1.0f);
    upper.ConnectedAnchor    = HM::Vector3(0.0f, 0.0f, 10.0f);
    JointComponent& lower    = scene.Join(bottom, JointType::Distance, top);
    lower.Anchor             = HM::Vector3(0.0f, 0.0f, 1.0f);
    lower.ConnectedAnchor    = HM::Vector3(0.0f, 0.0f, -1.0f);
    const std::string before = scene.Scenes.CaptureSnapshot().Yaml;

    Problems problems;
    REQUIRE(scene.Context.Play());
    scene.Frames(10);
    REQUIRE(scene.Physics().GetJointCount() == 2);
    const HP::ConstraintHandle lowerJoint = scene.Physics().GetJoint(bottom);

    scene.Scenes.DeleteGameObject(top);
    scene.Frames(30);
    CHECK_FALSE(scene.World().IsValid(lowerJoint));
    CHECK(scene.Physics().GetJointCount() == 0);
    CHECK(scene.Position(bottom).z() < 7.5f); // half a second of falling, about 1.2 m
    CHECK(problems.Errors.empty());
    CHECK(problems.Warnings.empty());

    // A joint added during Play holds from the next step.
    const ECS::Entity added = scene.Box(HM::Vector3(5.0f, 0.0f, 5.0f), HM::Vector3(0.2f, 0.2f, 0.2f), 1.0f);
    scene.Join(added, JointType::Fixed);
    scene.Frames(1);
    CHECK(scene.World().IsValid(scene.Physics().GetJoint(added)));
    scene.Frames(30);
    CHECK(scene.Position(added).z() == doctest::Approx(5.0f).epsilon(0.002));

    REQUIRE(scene.Context.Stop());
    CHECK(scene.Scenes.CaptureSnapshot().Yaml == before);
}

TEST_CASE("Joints - a rebuilt body, an edited joint, and a joint with two static sides")
{
    JointScene scene;
    const Door door(scene, false);
    const ECS::Entity wall  = scene.Box(HM::Vector3(10.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    scene.Join(wall, JointType::Fixed);

    Problems problems;
    REQUIRE(scene.Context.Play());
    scene.Frames(2);
    CHECK(problems.Warnings.find("Entity " + std::to_string(wall) + "'s joint is not made: both of its sides are static") !=
          std::string::npos);
    const HP::ConstraintHandle first = scene.Physics().GetJoint(door.Panel);
    REQUIRE(scene.World().IsValid(first));

    // The panel's body made again takes a new joint at once.
    scene.Physics().RebuildBody(scene.Ecs, door.Panel);
    const HP::ConstraintHandle second = scene.Physics().GetJoint(door.Panel);
    CHECK(scene.World().IsValid(second));
    CHECK_FALSE(scene.World().IsValid(first));

    // Limits set during Play take effect through RebuildJoint.
    JointComponent& hinge = scene.Ecs.GetComponent<JointComponent>(door.Panel);
    hinge.LimitsEnabled   = true;
    hinge.MinLimit        = -10.0f;
    hinge.MaxLimit        = 10.0f;
    scene.Physics().RebuildJoint(scene.Ecs, door.Panel);
    scene.Physics().AddImpulse(door.Panel, HM::Vector3(0.0f, 1.5f, 0.0f));
    scene.Frames(60);
    CHECK(std::abs(scene.World().GetConstraintAngle(scene.Physics().GetJoint(door.Panel))) < 12.0f);

    // A disabled joint goes; the warning comes once per Play.
    hinge.Enabled = false;
    scene.Frames(1);
    CHECK_FALSE(scene.Physics().GetJoint(door.Panel).IsSet());
    size_t warnings = 0;
    for (size_t at = problems.Warnings.find("both of its sides"); at != std::string::npos;
         at = problems.Warnings.find("both of its sides", at + 1))
        ++warnings;
    CHECK(warnings == 1);
    REQUIRE(scene.Context.Stop());
}
