#include "doctest/doctest/doctest.h"

#include "test_arm_gltf.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::EngineContext;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    struct RootMotionWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs = Context.GetECS();

        RootMotionWorld()
        {
            Dir.WriteFile("Arm.gltf", ArmTest::MakeArmGltf());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("arm://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));
        }

        ECS::Entity Object(const HM::Vector3& position)
        {
            const ECS::Entity entity = Context.GetSceneManager().CreateGameObject();
            Ecs.GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
            return entity;
        }

        // The arm playing clip, applying root motion as apply says.
        ECS::Entity Arm(const std::string& clip, bool apply, const HM::Vector3& position = HM::Vector3(0.0f, 0.0f, 0.0f))
        {
            const ECS::Entity arm = Object(position);
            Ecs.AddComponent(arm, HedgehogEngine::MeshComponent{});
            Context.GetMeshSystem()->LoadMesh(Ecs, arm, "arm://Arm.gltf");
            AnimatorComponent animator;
            animator.Clip            = clip;
            animator.ApplyRootMotion = apply;
            Ecs.AddComponent(arm, animator);
            Context.GetResourceCatalog().Update(*Context.GetRenderSystem(), *Context.GetMeshSystem());
            return arm;
        }

        void Frames(int count)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        TransformComponent&       Transform(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity); }
        AnimatorComponent&        Animator(ECS::Entity entity) { return Ecs.GetComponent<AnimatorComponent>(entity); }
        HM::Vector3               Shoulder(ECS::Entity entity)
        {
            const HM::Matrix4x4& model = Animator(entity).Palette[0];
            return HM::Vector3(model[3].x(), model[3].y(), model[3].z());
        }
    };
}

TEST_CASE("Root motion - a walk moves its entity while the skin's root stays at its bind pose")
{
    RootMotionWorld   world;
    const ECS::Entity arm = world.Arm("Walk", true, HM::Vector3(1.0f, 2.0f, 3.0f));
    REQUIRE(world.Context.Play());
    world.Frames(61); // the first frame shows time 0; the next 60 play a second

    const HM::Vector3 position = world.Transform(arm).Position;
    CHECK(position.x() == doctest::Approx(3.0f).epsilon(1e-3));
    CHECK(position.y() == doctest::Approx(2.0f));
    CHECK(position.z() == doctest::Approx(3.0f));
    CHECK(world.Animator(arm).RootMotion.x() == doctest::Approx(2.0f * STEP).epsilon(1e-3));
    // The shoulder's movement went to the entity: on the masked axes it is at its bind pose.
    CHECK(std::abs(world.Shoulder(arm).x()) < 1e-5f);

    // It keeps walking as the clip loops.
    world.Frames(60);
    CHECK(world.Transform(arm).Position.x() == doctest::Approx(5.0f).epsilon(1e-3));

    // Stop puts the transform back.
    REQUIRE(world.Context.Stop());
    CHECK(world.Transform(arm).Position == HM::Vector3(1.0f, 2.0f, 3.0f));
}

TEST_CASE("Root motion - off, the skin moves and the entity does not; a turn turns the entity")
{
    RootMotionWorld   world;
    const ECS::Entity still  = world.Arm("Walk", false);
    const ECS::Entity turner = world.Arm("Turn", true);
    REQUIRE(world.Context.Play());
    world.Frames(31);

    CHECK(world.Transform(still).Position == HM::Vector3(0.0f, 0.0f, 0.0f));
    CHECK(world.Shoulder(still).x() == doctest::Approx(1.0f).epsilon(1e-3));
    CHECK(world.Animator(still).RootMotion == HM::Vector3(0.0f, 0.0f, 0.0f));

    // Half a second of a quarter turn about the mesh's +y: 45 degrees on the entity.
    const HM::Quaternion turned = HM::Quaternion::FromEuler(world.Transform(turner).Rotation);
    const HM::Quaternion half   = HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 1.0f, 0.0f), 45.0f);
    CHECK(std::abs(std::abs(HM::Dot(turned, half)) - 1.0f) < 1e-4f);
    REQUIRE(world.Context.Stop());
    CHECK(world.Transform(turner).Rotation == HM::Vector3(0.0f, 0.0f, 0.0f));
}

TEST_CASE("Root motion - a character walking into a wall stops at the wall")
{
    RootMotionWorld   world;
    const ECS::Entity floor = world.Object(HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Transform(floor).Scale = HM::Vector3(50.0f, 50.0f, 0.5f);
    world.Ecs.AddComponent(floor, HedgehogEngine::ColliderComponent{});
    const ECS::Entity wall = world.Object(HM::Vector3(3.0f, 0.0f, 2.0f));
    world.Transform(wall).Scale = HM::Vector3(0.5f, 5.0f, 1.5f); // its face at x = 2.5
    world.Ecs.AddComponent(wall, HedgehogEngine::ColliderComponent{});

    const ECS::Entity character = world.Arm("Walk", true, HM::Vector3(0.0f, 0.0f, 1.5f));
    world.Ecs.AddComponent(character, HedgehogEngine::CharacterControllerComponent{});
    REQUIRE(world.Context.Play());

    world.Frames(30);
    const float halfway = world.Transform(character).Position.x();
    CHECK(halfway > 0.8f); // walking at 2 m/s, a frame late
    CHECK(world.Animator(character).PendingRootMotion.x() == doctest::Approx(2.0f).epsilon(1e-2));

    world.Frames(150);
    const float stopped = world.Transform(character).Position.x();
    CHECK(stopped < 2.5f - 0.39f);
    CHECK(stopped > 2.5f - 0.5f);
    REQUIRE(world.Context.Stop());
    CHECK(world.Transform(character).Position == HM::Vector3(0.0f, 0.0f, 1.5f));
}
