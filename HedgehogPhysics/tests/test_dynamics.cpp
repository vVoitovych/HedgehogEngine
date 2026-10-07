#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include <cmath>
#include <limits>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::MotionType;
using HP::PhysicsWorld;
using HP::PhysicsWorldDesc;
using HP::ShapeType;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    struct World
    {
        PhysicsWorld Physics;

        World() { REQUIRE(Physics.Init(PhysicsWorldDesc{})); }

        // A sphere gravity and damping leave alone, so only what the test does moves it.
        BodyHandle Body(MotionType motion, float mass = 2.0f)
        {
            BodyDesc desc;
            desc.Shape.Type     = ShapeType::Sphere;
            desc.Motion         = motion;
            desc.Mass           = mass;
            desc.GravityFactor  = 0.0f;
            desc.LinearDamping  = 0.0f;
            desc.AngularDamping = 0.0f;
            return Physics.CreateBody(desc);
        }
    };

    void CheckVector(const HM::Vector3& actual, const HM::Vector3& expected)
    {
        CHECK(actual.x() == doctest::Approx(expected.x()).epsilon(1e-4));
        CHECK(actual.y() == doctest::Approx(expected.y()).epsilon(1e-4));
        CHECK(actual.z() == doctest::Approx(expected.z()).epsilon(1e-4));
    }
}

TEST_CASE("Dynamics - an impulse changes velocity by J/m at once, a force by F dt/m over one step")
{
    World            world;
    const BodyHandle ball = world.Body(MotionType::Dynamic);
    CHECK(world.Physics.GetMass(ball) == doctest::Approx(2.0f));

    world.Physics.AddImpulse(ball, HM::Vector3(4.0f, 0.0f, 0.0f));
    CheckVector(world.Physics.GetLinearVelocity(ball), HM::Vector3(2.0f, 0.0f, 0.0f));

    world.Physics.AddForce(ball, HM::Vector3(0.0f, 6.0f, 0.0f));
    REQUIRE(world.Physics.Step(STEP));
    CheckVector(world.Physics.GetLinearVelocity(ball), HM::Vector3(2.0f, 6.0f * STEP / 2.0f, 0.0f));

    // Jolt clears the force after the step: the velocity holds.
    REQUIRE(world.Physics.Step(STEP));
    CheckVector(world.Physics.GetLinearVelocity(ball), HM::Vector3(2.0f, 6.0f * STEP / 2.0f, 0.0f));
}

TEST_CASE("Dynamics - velocities round-trip, and torques and off-centre pushes turn a body")
{
    World            world;
    const BodyHandle ball = world.Body(MotionType::Dynamic);

    world.Physics.SetLinearVelocity(ball, HM::Vector3(1.0f, -2.0f, 3.0f));
    world.Physics.SetAngularVelocity(ball, HM::Vector3(0.5f, 0.0f, -0.25f));
    CheckVector(world.Physics.GetLinearVelocity(ball), HM::Vector3(1.0f, -2.0f, 3.0f));
    CheckVector(world.Physics.GetAngularVelocity(ball), HM::Vector3(0.5f, 0.0f, -0.25f));

    world.Physics.SetAngularVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Physics.AddAngularImpulse(ball, HM::Vector3(0.0f, 0.0f, 1.0f));
    CHECK(world.Physics.GetAngularVelocity(ball).z() > 0.0f);

    world.Physics.SetAngularVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Physics.AddTorque(ball, HM::Vector3(0.0f, 0.0f, 1.0f));
    REQUIRE(world.Physics.Step(STEP));
    CHECK(world.Physics.GetAngularVelocity(ball).z() > 0.0f);

    // Pushing the sphere's +Y side along +X turns it about -Z and moves it along +X.
    world.Physics.SetLinearVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Physics.SetAngularVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    const HM::Vector3 side = world.Physics.GetPose(ball).Position + HM::Vector3(0.0f, 0.5f, 0.0f);
    world.Physics.AddImpulseAtPoint(ball, HM::Vector3(1.0f, 0.0f, 0.0f), side);
    CHECK(world.Physics.GetLinearVelocity(ball).x() == doctest::Approx(0.5f));
    CHECK(world.Physics.GetAngularVelocity(ball).z() < 0.0f);

    world.Physics.SetLinearVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Physics.SetAngularVelocity(ball, HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Physics.AddForceAtPoint(ball, HM::Vector3(60.0f, 0.0f, 0.0f), side);
    REQUIRE(world.Physics.Step(STEP));
    CHECK(world.Physics.GetLinearVelocity(ball).x() == doctest::Approx(60.0f * STEP / 2.0f).epsilon(1e-3));
    CHECK(world.Physics.GetAngularVelocity(ball).z() < 0.0f);
}

TEST_CASE("Dynamics - a sleeping body wakes when pushed")
{
    World            world;
    const BodyHandle ball = world.Body(MotionType::Dynamic);
    for (int step = 0; step < 120; ++step)
        REQUIRE(world.Physics.Step(STEP));
    std::vector<BodyHandle> active;
    world.Physics.GetActiveBodies(active);
    REQUIRE(active.empty());

    world.Physics.AddImpulse(ball, HM::Vector3(0.0f, 0.0f, 2.0f));
    REQUIRE(world.Physics.Step(STEP));
    CHECK(world.Physics.GetPose(ball).Position.z() > 0.0f);
}

TEST_CASE("Dynamics - forces leave static and kinematic bodies alone, and kinematic ones take velocities")
{
    World            world;
    const BodyHandle wall   = world.Body(MotionType::Static);
    const BodyHandle paddle = world.Body(MotionType::Kinematic);
    const HM::Vector3 push(10.0f, 0.0f, 0.0f);

    for (const BodyHandle body : { wall, paddle })
    {
        world.Physics.AddForce(body, push);
        world.Physics.AddForceAtPoint(body, push, HM::Vector3(0.0f, 1.0f, 0.0f));
        world.Physics.AddTorque(body, push);
        world.Physics.AddImpulse(body, push);
        world.Physics.AddImpulseAtPoint(body, push, HM::Vector3(0.0f, 1.0f, 0.0f));
        world.Physics.AddAngularImpulse(body, push);
        CHECK(world.Physics.GetMass(body) == 0.0f);
    }
    world.Physics.SetLinearVelocity(wall, push);
    REQUIRE(world.Physics.Step(STEP));
    for (const BodyHandle body : { wall, paddle })
    {
        CheckVector(world.Physics.GetLinearVelocity(body), HM::Vector3(0.0f, 0.0f, 0.0f));
        CheckVector(world.Physics.GetAngularVelocity(body), HM::Vector3(0.0f, 0.0f, 0.0f));
        CheckVector(world.Physics.GetPose(body).Position, HM::Vector3(0.0f, 0.0f, 0.0f));
    }

    world.Physics.SetLinearVelocity(paddle, HM::Vector3(0.0f, 0.0f, 3.0f));
    REQUIRE(world.Physics.Step(STEP));
    CHECK(world.Physics.GetPose(paddle).Position.z() == doctest::Approx(3.0f * STEP));
}

TEST_CASE("Dynamics - stale handles and non-finite values change nothing")
{
    World            world;
    const BodyHandle ball = world.Body(MotionType::Dynamic);
    const BodyHandle gone = world.Body(MotionType::Dynamic);
    world.Physics.DestroyBody(gone);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    world.Physics.AddImpulse(ball, HM::Vector3(nan, 0.0f, 0.0f));
    world.Physics.SetLinearVelocity(ball, HM::Vector3(0.0f, nan, 0.0f));
    world.Physics.AddForce(ball, HM::Vector3(0.0f, 0.0f, std::numeric_limits<float>::infinity()));
    REQUIRE(world.Physics.Step(STEP));
    CheckVector(world.Physics.GetLinearVelocity(ball), HM::Vector3(0.0f, 0.0f, 0.0f));

    for (const BodyHandle body : { gone, BodyHandle{} })
    {
        world.Physics.AddImpulse(body, HM::Vector3(1.0f, 0.0f, 0.0f));
        world.Physics.SetLinearVelocity(body, HM::Vector3(1.0f, 0.0f, 0.0f));
        CheckVector(world.Physics.GetLinearVelocity(body), HM::Vector3(0.0f, 0.0f, 0.0f));
        CHECK(world.Physics.GetMass(body) == 0.0f);
    }
}
