#include "doctest/doctest/doctest.h"

#include "test_arm_gltf.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <limits>
#include <memory>
#include <string>
#include <vector>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::EngineContext;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Every warning while it lives.
    class Warnings
    {
    public:
        Warnings()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel level, const std::string& line) {
                  if (level == EngineLogger::LogLevel::Warning)
                      Text += line + '\n';
              }))
        {
        }
        ~Warnings() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }
        Warnings(const Warnings&)            = delete;
        Warnings& operator=(const Warnings&) = delete;

        size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = Text.find(fragment); at != std::string::npos; at = Text.find(fragment, at + 1))
                ++count;
            return count;
        }

        std::string Text;

    private:
        int m_Sink;
    };

    // The arm on an entity with an animator that plays nothing, in Edit mode.
    struct ArmWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs = Context.GetECS();
        ECS::Entity   Arm{};

        ArmWorld()
        {
            Dir.WriteFile("Arm.gltf", ArmTest::MakeArmGltf());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("arm://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));
            Arm = Context.GetSceneManager().CreateGameObject();
            Ecs.AddComponent(Arm, HedgehogEngine::MeshComponent{});
            Context.GetMeshSystem()->LoadMesh(Ecs, Arm, "arm://Arm.gltf");
            Ecs.AddComponent(Arm, AnimatorComponent{});
            Context.GetResourceCatalog().Update(*Context.GetRenderSystem(), *Context.GetMeshSystem());
        }

        void Place(const HM::Vector3& position, const HM::Vector3& rotation)
        {
            auto& transform    = Ecs.GetComponent<TransformComponent>(Arm);
            transform.Position = position;
            transform.Rotation = rotation;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ Arm });
        }

        // Frames of the whole update: the transform phase gives the entity's matrix, which the next
        // evaluation uses.
        void Frames(int count)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        HedgehogEngine::AnimationSystem& Animations() { return *Context.GetAnimationSystem(); }
        const std::vector<HM::Matrix4x4>& Palette() { return Ecs.GetComponent<AnimatorComponent>(Arm).Palette; }

        // The hand in world space: its skinning matrix is its model matrix (no inverse bind).
        HM::Vector3 Hand()
        {
            REQUIRE(Palette().size() == 3u);
            const HM::Matrix4x4& model = Palette()[ArmTest::HAND];
            const HM::Vector4    world = Ecs.GetComponent<TransformComponent>(Arm).ObjMatrix *
                                      HM::Vector4(model[3].x(), model[3].y(), model[3].z(), 1.0f);
            return HM::Vector3(world.x(), world.y(), world.z());
        }
    };
}

TEST_CASE("Animation IK - a goal puts the hand on its world target, also with the entity moved and turned")
{
    ArmWorld world;
    world.Frames(1);
    const std::vector<HM::Matrix4x4> bind = world.Palette();

    const HM::Vector3 target(1.2f, 0.9f, 0.3f);
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Hand", target, 1.0f));
    world.Frames(1);
    CHECK((world.Hand() - target).LengthSlow() < 1e-3f);

    // Moved and turned a quarter about z: the target stays put in the world, and so does the hand.
    world.Place(HM::Vector3(5.0f, 2.0f, 1.0f), HM::Vector3(0.0f, 0.0f, 90.0f));
    const HM::Vector3 moved(5.5f, 3.2f, 1.4f);
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Hand", moved, 1.0f, HM::Vector3(5.0f, 4.0f, 3.0f)));
    world.Frames(2);
    CHECK((world.Hand() - moved).LengthSlow() < 1e-3f);

    // Cleared, the arm is back in the clip's (here the bind) pose.
    world.Place(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));
    CHECK(world.Animations().ClearIkGoal(world.Ecs, world.Arm, "Hand"));
    CHECK_FALSE(world.Animations().ClearIkGoal(world.Ecs, world.Arm, "Hand"));
    world.Frames(1);
    for (size_t joint = 0; joint < bind.size(); ++joint)
        for (size_t column = 0; column < 4; ++column)
            for (size_t row = 0; row < 4; ++row)
                CHECK(world.Palette()[joint][column][row] == doctest::Approx(bind[joint][column][row]));
}

TEST_CASE("Animation IK - an unknown joint warns once, bad goals are refused, and a fifth goal too")
{
    ArmWorld world;
    Warnings warnings;
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Foot", HM::Vector3(1.0f, 0.0f, 0.0f), 1.0f));
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Elbow", HM::Vector3(1.0f, 0.0f, 0.0f), 1.0f));
    world.Frames(3);
    CHECK(warnings.Count("IK goal names joint 'Foot'") == 1u);
    CHECK(warnings.Count("IK goal names joint 'Elbow'") == 1u); // one ancestor only

    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Hand", HM::Vector3(nan, 0.0f, 0.0f), 1.0f));
    CHECK_FALSE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Hand", HM::Vector3(1.0f, 0.0f, 0.0f), 1.5f));
    CHECK_FALSE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "", HM::Vector3(1.0f, 0.0f, 0.0f), 1.0f));
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "A", HM::Vector3(), 1.0f));
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "B", HM::Vector3(), 1.0f));
    CHECK_FALSE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "C", HM::Vector3(), 1.0f));
    CHECK(world.Animations().SetIkGoal(world.Ecs, world.Arm, "A", HM::Vector3(1.0f, 1.0f, 1.0f), 0.5f)); // replaces A
    CHECK(world.Ecs.GetComponent<AnimatorComponent>(world.Arm).IkGoals.size() == 4u);
}

TEST_CASE("Animation IK - goals set during Play are gone after Stop")
{
    ArmWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Animations().SetIkGoal(world.Ecs, world.Arm, "Hand", HM::Vector3(1.0f, 1.0f, 0.0f), 1.0f));
    world.Frames(2);
    REQUIRE(world.Context.Stop());
    CHECK(world.Ecs.GetComponent<AnimatorComponent>(world.Arm).IkGoals.empty());
}
