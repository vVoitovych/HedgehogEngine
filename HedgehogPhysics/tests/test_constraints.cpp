#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::ConstraintDesc;
using HP::ConstraintHandle;
using HP::ConstraintType;
using HP::MotionType;
using HP::PhysicsWorld;
using HP::ShapeType;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    float Distance(const HM::Vector3& a, const HM::Vector3& b) { return (a - b).LengthSlow(); }

    struct World
    {
        PhysicsWorld Physics;

        explicit World(int32_t workerThreads = 0)
        {
            HP::PhysicsWorldDesc desc;
            desc.WorkerThreads = workerThreads;
            REQUIRE(Physics.Init(desc));
        }

        void Steps(int count)
        {
            for (int step = 0; step < count; ++step)
                REQUIRE(Physics.Step(STEP));
        }

        // An undamped dynamic body: a 1 m box, or a sphere of radius 0.2.
        BodyHandle Body(const HM::Vector3& position, ShapeType shape = ShapeType::Box)
        {
            BodyDesc desc;
            desc.Shape.Type     = shape;
            desc.Shape.Radius   = 0.2f;
            desc.Pose.Position  = position;
            desc.LinearDamping  = 0.0f;
            desc.AngularDamping = 0.0f;
            return Physics.CreateBody(desc);
        }

        HM::Vector3 Position(BodyHandle body) const { return Physics.GetPose(body).Position; }
    };

    ConstraintDesc Joint(ConstraintType type, BodyHandle a, BodyHandle b, const HM::Vector3& anchor)
    {
        ConstraintDesc desc;
        desc.Type   = type;
        desc.BodyA  = a;
        desc.BodyB  = b;
        desc.Anchor = anchor;
        return desc;
    }

    // A ball on a 1 m rod from a world hinge at (0, 0, 5) about +Y, let go level.
    struct Pendulum
    {
        BodyHandle       Ball;
        ConstraintHandle Hinge;

        Pendulum(World& world, bool limited)
        {
            Ball                = world.Body(HM::Vector3(1.0f, 0.0f, 5.0f), ShapeType::Sphere);
            ConstraintDesc desc = Joint(ConstraintType::Hinge, {}, Ball, HM::Vector3(0.0f, 0.0f, 5.0f));
            desc.Axis           = HM::Vector3(0.0f, 2.0f, 0.0f);
            desc.LimitsEnabled  = limited;
            desc.MinLimit       = -45.0f;
            desc.MaxLimit       = 45.0f;
            Hinge               = world.Physics.CreateConstraint(desc);
            REQUIRE(world.Physics.IsValid(Hinge));
        }
    };
}

TEST_CASE("Constraints - a box fixed to the world stays, two fixed together fall as one")
{
    World            world;
    const BodyHandle held = world.Body(HM::Vector3(-5.0f, 0.0f, 5.0f));
    REQUIRE(world.Physics.IsValid(world.Physics.CreateConstraint(
        Joint(ConstraintType::Fixed, held, {}, HM::Vector3(-5.0f, 0.0f, 5.0f)))));

    const BodyHandle a = world.Body(HM::Vector3(0.0f, 0.0f, 5.0f));
    const BodyHandle b = world.Body(HM::Vector3(1.5f, 0.0f, 5.5f));
    REQUIRE(world.Physics.IsValid(world.Physics.CreateConstraint(
        Joint(ConstraintType::Fixed, a, b, HM::Vector3(0.75f, 0.0f, 5.25f)))));
    CHECK(world.Physics.GetConstraintCount() == 2);

    const float apart = Distance(world.Position(a), world.Position(b));
    for (int step = 0; step < 60; ++step)
    {
        world.Steps(1);
        CHECK(Distance(world.Position(a), world.Position(b)) == doctest::Approx(apart).epsilon(0.01));
    }
    CHECK(world.Position(held).z() == doctest::Approx(5.0f).epsilon(0.002));
    // A second of falling: about 4.9 m, side by side.
    CHECK(world.Position(a).z() < 1.0f);
    CHECK(world.Position(b).z() - world.Position(a).z() == doctest::Approx(0.5f).epsilon(0.02));
}

TEST_CASE("Constraints - a hinge swings about its axis only, and stops at its limits")
{
    World          world;
    const Pendulum free(world, false);
    float          lowest = 5.0f;
    for (int step = 0; step < 60; ++step)
    {
        world.Steps(1);
        const HM::Vector3 ball = world.Position(free.Ball);
        CHECK(std::abs(ball.y()) < 1e-3f);
        CHECK(Distance(ball, HM::Vector3(0.0f, 0.0f, 5.0f)) == doctest::Approx(1.0f).epsilon(0.01));
        lowest = std::min(lowest, ball.z());
    }
    // It swung through the bottom, a full quarter turn from level.
    CHECK(lowest == doctest::Approx(4.0f).epsilon(0.01));

    World          limitedWorld;
    const Pendulum limited(limitedWorld, true);
    // It swings into the limit (overshooting it for a step or two) and comes to rest against it.
    limitedWorld.Steps(120);
    float widest = 0.0f;
    for (int step = 0; step < 60; ++step)
    {
        limitedWorld.Steps(1);
        widest = std::max(widest, std::abs(limitedWorld.Physics.GetConstraintAngle(limited.Hinge)));
    }
    CHECK(widest < 47.0f);
    CHECK(std::abs(limitedWorld.Physics.GetConstraintAngle(limited.Hinge)) == doctest::Approx(45.0f).epsilon(2.0 / 45.0));
}

TEST_CASE("Constraints - a slider moves only along its axis and stops at its limit")
{
    World            world;
    const BodyHandle box  = world.Body(HM::Vector3(0.0f, 0.0f, 5.0f));
    ConstraintDesc   desc = Joint(ConstraintType::Slider, {}, box, HM::Vector3(0.0f, 0.0f, 5.0f));
    // Down a 45 degree slope along +x.
    desc.Axis                    = HM::Vector3(1.0f, 0.0f, -1.0f);
    desc.LimitsEnabled           = true;
    desc.MinLimit                = -1.0f;
    desc.MaxLimit                = 2.0f;
    const ConstraintHandle slider = world.Physics.CreateConstraint(desc);
    REQUIRE(world.Physics.IsValid(slider));

    for (int step = 0; step < 120; ++step)
    {
        world.Steps(1);
        const HP::BodyPose pose = world.Physics.GetPose(box);
        CHECK(std::abs(pose.Position.y()) < 1e-3f);
        CHECK(pose.Position.x() == doctest::Approx(5.0f - pose.Position.z()).epsilon(1e-3));
        CHECK(std::abs(pose.Rotation.w()) == doctest::Approx(1.0f).epsilon(1e-3));
    }
    CHECK(world.Physics.GetConstraintPosition(slider) == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(world.Position(box).x() == doctest::Approx(2.0f / std::sqrt(2.0f)).epsilon(0.01));
}

TEST_CASE("Constraints - a distance holds its length, and a spring oscillates then settles")
{
    World            world;
    const BodyHandle ball  = world.Body(HM::Vector3(2.0f, 0.0f, 5.0f), ShapeType::Sphere);
    ConstraintDesc   desc  = Joint(ConstraintType::Distance, {}, ball, HM::Vector3(0.0f, 0.0f, 5.0f));
    desc.AnchorB           = HM::Vector3(2.0f, 0.0f, 5.0f);
    const ConstraintHandle rope = world.Physics.CreateConstraint(desc);
    REQUIRE(world.Physics.IsValid(rope));
    for (int step = 0; step < 120; ++step)
    {
        world.Steps(1);
        CHECK(world.Physics.GetConstraintPosition(rope) == doctest::Approx(2.0f).epsilon(0.005));
        CHECK(Distance(world.Position(ball), HM::Vector3(0.0f, 0.0f, 5.0f)) == doctest::Approx(2.0f).epsilon(0.005));
    }

    // Hanging on a 2 Hz spring, it bobs about its stretched length and comes to rest there.
    World            springWorld;
    const BodyHandle weight = springWorld.Body(HM::Vector3(0.0f, 0.0f, 3.0f), ShapeType::Sphere);
    desc                    = Joint(ConstraintType::Distance, {}, weight, HM::Vector3(0.0f, 0.0f, 5.0f));
    desc.AnchorB            = HM::Vector3(0.0f, 0.0f, 3.0f);
    desc.SpringFrequency    = 2.0f;
    desc.SpringDamping      = 0.3f;
    const ConstraintHandle spring = springWorld.Physics.CreateConstraint(desc);
    REQUIRE(springWorld.Physics.IsValid(spring));
    float shortest = 10.0f, longest = 0.0f;
    for (int step = 0; step < 60; ++step)
    {
        springWorld.Steps(1);
        const float length = springWorld.Physics.GetConstraintPosition(spring);
        shortest           = std::min(shortest, length);
        longest            = std::max(longest, length);
    }
    CHECK(longest > 2.07f);
    CHECK(shortest < 2.06f);
    springWorld.Steps(480);
    shortest = 10.0f;
    longest  = 0.0f;
    for (int step = 0; step < 60; ++step)
    {
        springWorld.Steps(1);
        const float length = springWorld.Physics.GetConstraintPosition(spring);
        shortest           = std::min(shortest, length);
        longest            = std::max(longest, length);
    }
    CHECK(longest - shortest < 0.005f);
    // g / (2 pi f)^2 of stretch.
    CHECK(longest == doctest::Approx(2.0f + 9.81f / std::pow(2.0f * 3.14159265f * 2.0f, 2.0f)).epsilon(0.005));
}

TEST_CASE("Constraints - destroying a body or a constraint removes it")
{
    World          world;
    const Pendulum pendulum(world, false);
    world.Physics.DestroyBody(pendulum.Ball);
    CHECK_FALSE(world.Physics.IsValid(pendulum.Hinge));
    CHECK(world.Physics.GetConstraintCount() == 0);
    CHECK(world.Physics.GetConstraintAngle(pendulum.Hinge) == 0.0f);
    world.Steps(10);

    // A destroyed constraint lets go of its body, and its slot's next one is a new handle.
    const BodyHandle       box   = world.Body(HM::Vector3(0.0f, 0.0f, 5.0f));
    const ConstraintHandle fixed = world.Physics.CreateConstraint(Joint(ConstraintType::Fixed, box, {}, HM::Vector3()));
    world.Steps(30);
    CHECK(world.Position(box).z() == doctest::Approx(5.0f).epsilon(0.002));
    world.Physics.DestroyConstraint(fixed);
    CHECK_FALSE(world.Physics.IsValid(fixed));
    world.Steps(30);
    CHECK(world.Position(box).z() < 4.0f);
    const ConstraintHandle again = world.Physics.CreateConstraint(Joint(ConstraintType::Fixed, box, {}, HM::Vector3()));
    CHECK(again.Index == fixed.Index);
    CHECK_FALSE(again == fixed);
    CHECK_FALSE(world.Physics.IsValid(fixed));

    // DestroyAllBodies takes the constraints with them; a world shut down with some is fine too.
    world.Physics.DestroyAllBodies();
    CHECK(world.Physics.GetConstraintCount() == 0);
    const Pendulum kept(world, true);
    world.Physics.Shutdown();
    CHECK_FALSE(world.Physics.IsValid(kept.Hinge));
}

TEST_CASE("Constraints - bad descs are refused with one error each")
{
    World            world;
    const BodyHandle a     = world.Body(HM::Vector3(0.0f, 0.0f, 5.0f));
    const BodyHandle stale = world.Body(HM::Vector3(3.0f, 0.0f, 5.0f));
    world.Physics.DestroyBody(stale);

    std::vector<ConstraintDesc> bad;
    bad.push_back(Joint(ConstraintType::Fixed, {}, {}, HM::Vector3()));
    bad.push_back(Joint(ConstraintType::Fixed, a, a, HM::Vector3()));
    bad.push_back(Joint(ConstraintType::Hinge, a, stale, HM::Vector3()));
    ConstraintDesc zeroAxis = Joint(ConstraintType::Slider, a, {}, HM::Vector3());
    zeroAxis.Axis           = HM::Vector3(0.0f, 0.0f, 0.0f);
    bad.push_back(zeroAxis);
    ConstraintDesc reversed = Joint(ConstraintType::Distance, a, {}, HM::Vector3());
    reversed.LimitsEnabled  = true;
    reversed.MinLimit       = 2.0f;
    reversed.MaxLimit       = 1.0f;
    bad.push_back(reversed);
    ConstraintDesc wideHinge = Joint(ConstraintType::Hinge, a, {}, HM::Vector3());
    wideHinge.LimitsEnabled  = true;
    wideHinge.MinLimit       = 10.0f;
    wideHinge.MaxLimit       = 45.0f;
    bad.push_back(wideHinge);
    ConstraintDesc spring  = Joint(ConstraintType::Distance, a, {}, HM::Vector3());
    spring.SpringFrequency = -1.0f;
    bad.push_back(spring);
    ConstraintDesc nan = Joint(ConstraintType::Fixed, a, {}, HM::Vector3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f));
    bad.push_back(nan);

    for (size_t index = 0; index < bad.size(); ++index)
    {
        CAPTURE(index);
        LogCapture log;
        CHECK_FALSE(world.Physics.CreateConstraint(bad[index]).IsSet());
        CHECK(log.Lines("[Physics] A constraint cannot be made").size() == 1);
    }
    CHECK(world.Physics.GetConstraintCount() == 0);

    PhysicsWorld stopped;
    LogCapture   log;
    CHECK_FALSE(stopped.CreateConstraint(Joint(ConstraintType::Fixed, a, {}, HM::Vector3())).IsSet());
    CHECK(log.Lines("the world is not running").size() == 1);
}

TEST_CASE("Constraints - a chain swings the same on the calling thread and on a pool")
{
    std::vector<HM::Vector3> positions[2];
    for (const int32_t threads : { 0, 2 })
    {
        CAPTURE(threads);
        World      world(threads);
        BodyHandle previous;
        for (int link = 0; link < 4; ++link)
        {
            const HM::Vector3 position(1.2f * static_cast<float>(link + 1), 0.0f, 8.0f);
            const BodyHandle  body = world.Body(position, ShapeType::Sphere);
            ConstraintDesc    desc = Joint(ConstraintType::Hinge, previous, body, position - HM::Vector3(1.2f, 0.0f, 0.0f));
            desc.Axis              = HM::Vector3(0.0f, 1.0f, 0.0f);
            REQUIRE(world.Physics.IsValid(world.Physics.CreateConstraint(desc)));
            previous = body;
        }
        world.Steps(120);
        positions[threads == 0 ? 0 : 1].push_back(world.Position(previous));
    }
    CHECK(positions[0][0].x() == doctest::Approx(positions[1][0].x()).epsilon(1e-4));
    CHECK(positions[0][0].z() == doctest::Approx(positions[1][0].z()).epsilon(1e-4));
}
