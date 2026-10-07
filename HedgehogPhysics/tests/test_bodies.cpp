#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::BodyPose;
using HP::MotionType;
using HP::PhysicsWorld;
using HP::PhysicsWorldDesc;
using HP::ShapeType;

namespace
{
    constexpr float STEP    = 1.0f / 60.0f;
    constexpr float GRAVITY = 9.81f;

    // A world that runs Jolt's jobs on the test's thread unless told otherwise.
    struct World
    {
        PhysicsWorld Physics;

        explicit World(int32_t workerThreads = 0, const HP::CollisionMatrix& matrix = HP::MakeFullCollisionMatrix())
        {
            PhysicsWorldDesc desc;
            desc.WorkerThreads = workerThreads;
            desc.Collisions    = matrix;
            REQUIRE(Physics.Init(desc));
        }

        void Steps(int count)
        {
            for (int step = 0; step < count; ++step)
                REQUIRE(Physics.Step(STEP));
        }

        // A static floor 10 x 10 m whose top face is at z = 0.5.
        BodyHandle Floor(uint32_t layer = 0)
        {
            BodyDesc desc;
            desc.Motion            = MotionType::Static;
            desc.Shape.HalfExtents = HM::Vector3(5.0f, 5.0f, 0.5f);
            desc.Layer             = layer;
            return Physics.CreateBody(desc);
        }

        BodyHandle Body(ShapeType type, const HM::Vector3& position, MotionType motion = MotionType::Dynamic)
        {
            BodyDesc desc;
            desc.Shape.Type    = type;
            desc.Motion        = motion;
            desc.Pose.Position = position;
            return Physics.CreateBody(desc);
        }

        float Z(BodyHandle body) const { return Physics.GetPose(body).Position.z(); }

        bool IsActive(BodyHandle body) const
        {
            std::vector<BodyHandle> active;
            Physics.GetActiveBodies(active);
            return std::find(active.begin(), active.end(), body) != active.end();
        }
    };
}

TEST_CASE("Bodies - a dynamic sphere falls as gravity says")
{
    World      world;
    BodyDesc   desc;
    desc.Shape.Type     = ShapeType::Sphere;
    desc.Pose.Position  = HM::Vector3(0.0f, 0.0f, 100.0f);
    desc.LinearDamping  = 0.0f;
    const BodyHandle ball = world.Physics.CreateBody(desc);
    REQUIRE(ball.IsSet());
    CHECK(world.Physics.GetBodyCount() == 1);

    constexpr int steps = 60;
    world.Steps(steps);
    const float fallen = 100.0f - world.Z(ball);
    // Jolt integrates velocity before position (symplectic Euler): exactly g dt^2 n(n + 1) / 2 ...
    CHECK(fallen == doctest::Approx(GRAVITY * STEP * STEP * steps * (steps + 1) / 2.0f).epsilon(0.001));
    // ... which is the continuous 0.5 g t^2 within (n + 1) / n, 1.7 % at 60 steps.
    const float t = steps * STEP;
    CHECK(fallen == doctest::Approx(0.5f * GRAVITY * t * t).epsilon(0.02));
}

TEST_CASE("Bodies - a box comes to rest on a static box, then sleeps")
{
    World            world;
    const BodyHandle floor = world.Floor();
    const BodyHandle box   = world.Body(ShapeType::Box, HM::Vector3(0.0f, 0.0f, 3.0f));
    REQUIRE(floor.IsSet());
    REQUIRE(box.IsSet());
    CHECK(world.Physics.GetMotionType(floor) == MotionType::Static);
    CHECK(world.Physics.GetMotionType(box) == MotionType::Dynamic);
    CHECK(world.IsActive(box));
    CHECK_FALSE(world.IsActive(floor)); // a static body never moves

    world.Steps(300);
    CHECK(world.Z(box) - 0.5f == doctest::Approx(0.5f).epsilon(0.02)); // its bottom on the top face, within 1 cm
    CHECK_FALSE(world.IsActive(box));
}

TEST_CASE("Bodies - each shape's size, centre and scale decide where it rests")
{
    World world;
    world.Floor();

    // A box scaled to 2 m tall rests with its centre 1 m above the floor.
    BodyDesc tall;
    tall.Pose.Position = HM::Vector3(-3.0f, 0.0f, 4.0f);
    tall.Shape.Scale   = HM::Vector3(1.0f, 1.0f, 2.0f);
    const BodyHandle tallBox = world.Physics.CreateBody(tall);

    // A box whose shape sits 1 m above its origin rests with its origin at the floor's top face - 0.5.
    BodyDesc raised;
    raised.Pose.Position = HM::Vector3(-1.5f, 0.0f, 4.0f);
    raised.Shape.Center  = HM::Vector3(0.0f, 0.0f, 1.0f);
    const BodyHandle raisedBox = world.Physics.CreateBody(raised);

    // A sphere of radius 0.5 scaled by 2 rests 1 m up.
    BodyDesc big;
    big.Shape.Type     = ShapeType::Sphere;
    big.Pose.Position  = HM::Vector3(0.0f, 0.0f, 4.0f);
    big.Shape.Scale    = HM::Vector3(2.0f, 2.0f, 2.0f);
    const BodyHandle bigSphere = world.Physics.CreateBody(big);

    // A capsule lies along local +Z: turned a quarter about X it lies flat, resting on its radius.
    BodyDesc lying;
    lying.Shape.Type       = ShapeType::Capsule;
    lying.Shape.Radius     = 0.25f;
    lying.Shape.HalfHeight = 0.5f;
    lying.Pose.Position    = HM::Vector3(2.0f, 0.0f, 3.0f);
    lying.Pose.Rotation    = HM::Quaternion::FromEuler(90.0f, 0.0f, 0.0f);
    lying.AngularDamping   = 0.5f;
    const BodyHandle capsule = world.Physics.CreateBody(lying);

    world.Steps(240);
    CHECK(world.Z(tallBox) == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(world.Z(raisedBox) == doctest::Approx(0.0f).scale(1.0f).epsilon(0.02));
    CHECK(world.Z(bigSphere) == doctest::Approx(1.5f).epsilon(0.02));
    CHECK(world.Z(capsule) == doctest::Approx(0.75f).epsilon(0.02));

    // An unevenly scaled sphere takes its largest axis, with a warning.
    LogCapture log;
    BodyDesc   uneven;
    uneven.Shape.Type  = ShapeType::Sphere;
    uneven.Shape.Scale = HM::Vector3(1.0f, 3.0f, 1.0f);
    uneven.Pose.Position = HM::Vector3(4.0f, 0.0f, 5.0f);
    const BodyHandle unevenSphere = world.Physics.CreateBody(uneven);
    CHECK(log.Lines("[WARNING]").size() == 1);
    world.Steps(240);
    CHECK(world.Z(unevenSphere) == doctest::Approx(2.0f).epsilon(0.02));
}

TEST_CASE("Bodies - a kinematic body reaches its target in one step and pushes a dynamic box")
{
    PhysicsWorld     physics;
    PhysicsWorldDesc desc;
    desc.Gravity = HM::Vector3(0.0f, 0.0f, 0.0f);
    REQUIRE(physics.Init(desc));

    BodyDesc paddleDesc;
    paddleDesc.Motion = MotionType::Kinematic;
    const BodyHandle paddle = physics.CreateBody(paddleDesc);
    BodyDesc boxDesc;
    boxDesc.Pose.Position = HM::Vector3(1.2f, 0.0f, 0.0f);
    const BodyHandle box  = physics.CreateBody(boxDesc);
    CHECK(physics.GetMotionType(paddle) == MotionType::Kinematic);

    BodyPose target;
    target.Position = HM::Vector3(0.1f, 0.0f, 0.0f);
    physics.MoveKinematic(paddle, target, STEP);
    REQUIRE(physics.Step(STEP));
    CHECK(physics.GetPose(paddle).Position.x() == doctest::Approx(0.1f).epsilon(1e-4));

    for (int step = 2; step <= 30; ++step)
    {
        target.Position = HM::Vector3(0.1f * static_cast<float>(step), 0.0f, 0.0f);
        physics.MoveKinematic(paddle, target, STEP);
        REQUIRE(physics.Step(STEP));
    }
    CHECK(physics.GetPose(paddle).Position.x() == doctest::Approx(3.0f).epsilon(1e-4));
    CHECK(physics.GetPose(box).Position.x() > 3.5f); // pushed ahead of the paddle

    // MoveKinematic does nothing to a dynamic body; SetPose teleports any body.
    BodyPose away;
    away.Position = HM::Vector3(9.0f, 9.0f, 9.0f);
    physics.MoveKinematic(box, away, STEP);
    REQUIRE(physics.Step(STEP));
    CHECK(physics.GetPose(box).Position.x() < 9.0f);
    physics.SetPose(box, away);
    CHECK(physics.GetPose(box).Position.y() == doctest::Approx(9.0f));
    CHECK(physics.GetPose(box).Position.z() == doctest::Approx(9.0f));
}

TEST_CASE("Bodies - layers the matrix keeps apart pass through each other")
{
    HP::CollisionMatrix matrix = HP::MakeFullCollisionMatrix();
    HP::SetCollides(matrix, 0, 1, false);
    World world(0, matrix);
    world.Floor(0);

    BodyDesc ghost;
    ghost.Pose.Position = HM::Vector3(0.0f, 0.0f, 3.0f);
    ghost.Layer         = 1;
    const BodyHandle throughFloor = world.Physics.CreateBody(ghost);
    const BodyHandle onFloor      = world.Body(ShapeType::Box, HM::Vector3(2.0f, 0.0f, 3.0f));

    world.Steps(120);
    CHECK(world.Z(throughFloor) < -1.0f);
    CHECK(world.Z(onFloor) == doctest::Approx(1.0f).epsilon(0.02));
}

TEST_CASE("Bodies - destroyed bodies' handles go stale, even when their index is reused")
{
    World            world;
    const BodyHandle first = world.Body(ShapeType::Sphere, HM::Vector3(0.0f, 0.0f, 0.0f));
    BodyDesc         tagged;
    tagged.UserData = 0x1234'5678'9abcULL;
    const BodyHandle second = world.Physics.CreateBody(tagged);
    CHECK(world.Physics.GetUserData(second) == 0x1234'5678'9abcULL);
    CHECK(world.Physics.GetBodyCount() == 2);

    world.Physics.DestroyBody(first);
    CHECK(world.Physics.GetBodyCount() == 1);
    CHECK_FALSE(world.Physics.IsValid(first));
    world.Physics.DestroyBody(first); // a second time: nothing

    const BodyHandle reused = world.Body(ShapeType::Box, HM::Vector3(0.0f, 0.0f, 0.0f));
    CHECK(world.Physics.IsValid(reused));
    CHECK_FALSE(world.Physics.IsValid(first));
    CHECK(reused != first);

    // Calls on a stale or unset handle do nothing and return defaults.
    BodyPose pose;
    pose.Position = HM::Vector3(1.0f, 2.0f, 3.0f);
    world.Physics.SetPose(first, pose);
    world.Physics.MoveKinematic(first, pose, STEP);
    CHECK(world.Physics.GetPose(first).Position.x() == 0.0f);
    CHECK(world.Physics.GetUserData(first) == 0);
    CHECK_FALSE(world.Physics.IsValid(BodyHandle{}));
    CHECK(world.Physics.GetPose(reused).Position.x() == 0.0f);

    world.Physics.DestroyAllBodies();
    CHECK(world.Physics.GetBodyCount() == 0);
    CHECK_FALSE(world.Physics.IsValid(second));
    CHECK_FALSE(world.Physics.IsValid(reused));
}

TEST_CASE("Bodies - a desc that cannot make a body is refused with one error")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    World       world;

    std::vector<BodyDesc> bad(7);
    bad[0].Shape.HalfExtents = HM::Vector3(0.5f, 0.0f, 0.5f);
    bad[1].Shape.Type        = ShapeType::Sphere;
    bad[1].Shape.Radius      = -1.0f;
    bad[2].Shape.Type        = ShapeType::Capsule;
    bad[2].Shape.HalfHeight  = -0.5f;
    bad[3].Layer             = HP::PHYSICS_LAYER_COUNT;
    bad[4].Pose.Position     = HM::Vector3(nan, 0.0f, 0.0f);
    bad[5].Mass              = 0.0f;
    bad[6].Shape.Scale       = HM::Vector3(1.0f, 0.0f, 1.0f);

    for (size_t index = 0; index < bad.size(); ++index)
    {
        CAPTURE(index);
        LogCapture log;
        CHECK_FALSE(world.Physics.CreateBody(bad[index]).IsSet());
        CHECK(log.Lines("[ERROR]").size() == 1);
    }
    CHECK(world.Physics.GetBodyCount() == 0);

    // A static body needs no mass; a stopped world makes nothing.
    BodyDesc massless;
    massless.Motion = MotionType::Static;
    massless.Mass   = 0.0f;
    CHECK(world.Physics.CreateBody(massless).IsSet());
    PhysicsWorld stopped;
    LogCapture   log;
    CHECK_FALSE(stopped.CreateBody(BodyDesc{}).IsSet());
    CHECK(log.Lines("[ERROR]").size() == 1);
}

TEST_CASE("Bodies - a stack settles to the same poses on the calling thread and on a pool")
{
    std::vector<BodyPose> poses[2];
    for (const int32_t threads : { 0, 2 })
    {
        CAPTURE(threads);
        World world(threads);
        world.Floor();
        std::vector<BodyHandle> stack;
        for (int level = 0; level < 3; ++level)
            stack.push_back(world.Body(ShapeType::Box, HM::Vector3(0.1f * level, 0.0f, 1.2f + 1.1f * level)));
        world.Steps(180);
        for (const BodyHandle box : stack)
            poses[threads == 0 ? 0 : 1].push_back(world.Physics.GetPose(box));
    }

    REQUIRE(poses[0].size() == poses[1].size());
    for (size_t index = 0; index < poses[0].size(); ++index)
    {
        CAPTURE(index);
        CHECK(poses[0][index].Position.x() == doctest::Approx(poses[1][index].Position.x()).epsilon(1e-4));
        CHECK(poses[0][index].Position.z() == doctest::Approx(poses[1][index].Position.z()).epsilon(1e-4));
    }
}
