#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <limits>
#include <memory>

using HP::CollisionMatrix;
using HP::PhysicsWorld;
using HP::PhysicsWorldDesc;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;
}

TEST_CASE("PhysicsWorld - starts, steps, stops, and stops again safely")
{
    PhysicsWorld world;
    world.Shutdown(); // before Init: nothing to do
    CHECK_FALSE(world.IsInitialized());
    CHECK_FALSE(world.Step(STEP));
    CHECK(world.GetBodyCount() == 0);

    REQUIRE(world.Init(PhysicsWorldDesc{}));
    CHECK(world.IsInitialized());
    CHECK(world.GetBodyCount() == 0);
    CHECK(world.GetGravity().z() == doctest::Approx(-9.81f));
    CHECK(world.Step(STEP));
    CHECK_FALSE(world.Step(0.0f));
    CHECK_FALSE(world.Step(-STEP));
    CHECK_FALSE(world.Step(std::numeric_limits<float>::quiet_NaN()));

    // Init over a running world starts it afresh.
    PhysicsWorldDesc other;
    other.Gravity = HM::Vector3(0.0f, -1.0f, 0.0f);
    REQUIRE(world.Init(other));
    CHECK(world.GetGravity().y() == doctest::Approx(-1.0f));

    world.Shutdown();
    world.Shutdown();
    CHECK_FALSE(world.IsInitialized());
    CHECK_FALSE(world.Step(STEP));

    // And it starts again after stopping.
    REQUIRE(world.Init(PhysicsWorldDesc{}));
    CHECK(world.Step(STEP));
}

TEST_CASE("PhysicsWorld - worlds live side by side and stop in any order")
{
    for (const bool firstStopsFirst : { true, false })
    {
        CAPTURE(firstStopsFirst);
        auto first  = std::make_unique<PhysicsWorld>();
        auto second = std::make_unique<PhysicsWorld>();
        REQUIRE(first->Init(PhysicsWorldDesc{}));
        REQUIRE(second->Init(PhysicsWorldDesc{}));
        CHECK(first->Step(STEP));
        CHECK(second->Step(STEP));

        (firstStopsFirst ? first : second).reset();
        PhysicsWorld& remaining = firstStopsFirst ? *second : *first;
        CHECK(remaining.Step(STEP));
        CHECK(remaining.IsInitialized());
    }

    // With every world gone, Jolt's setup is made again by the next.
    PhysicsWorld fresh;
    REQUIRE(fresh.Init(PhysicsWorldDesc{}));
    CHECK(fresh.Step(STEP));
}

TEST_CASE("PhysicsWorld - a desc it refuses leaves the world stopped with one error")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    PhysicsWorldDesc noBodies;
    noBodies.MaxBodies = 0;
    PhysicsWorldDesc tooMany;
    tooMany.MaxBodies = HP::MAX_BODIES_LIMIT + 1;
    PhysicsWorldDesc nanGravity;
    nanGravity.Gravity = HM::Vector3(0.0f, nan, 0.0f);
    PhysicsWorldDesc infGravity;
    infGravity.Gravity = HM::Vector3(inf, 0.0f, 0.0f);
    PhysicsWorldDesc negativeThreads;
    negativeThreads.WorkerThreads = -1;

    int index = 0;
    for (const PhysicsWorldDesc& desc : { noBodies, tooMany, nanGravity, infGravity, negativeThreads })
    {
        CAPTURE(index++);
        PhysicsWorld world;
        LogCapture   log;
        CHECK_FALSE(world.Init(desc));
        CHECK_FALSE(world.IsInitialized());
        CHECK(log.Lines("[ERROR]").size() == 1);
        CHECK(log.Lines("[Physics]").size() == 1);
    }

    // A running world refused a new desc is stopped, not left as it was.
    PhysicsWorld running;
    REQUIRE(running.Init(PhysicsWorldDesc{}));
    {
        LogCapture log;
        CHECK_FALSE(running.Init(noBodies));
    }
    CHECK_FALSE(running.IsInitialized());
}

TEST_CASE("PhysicsWorld - gravity and the collision matrix are set while it runs")
{
    PhysicsWorld world;
    REQUIRE(world.Init(PhysicsWorldDesc{}));

    world.SetGravity(HM::Vector3(1.0f, 2.0f, 3.0f));
    CHECK(world.GetGravity().x() == doctest::Approx(1.0f));
    CHECK(world.GetGravity().z() == doctest::Approx(3.0f));
    {
        LogCapture log;
        world.SetGravity(HM::Vector3(0.0f, 0.0f, std::numeric_limits<float>::infinity()));
        CHECK(log.Lines("[WARNING]").size() == 1);
    }
    CHECK(world.GetGravity().z() == doctest::Approx(3.0f));

    CollisionMatrix matrix = HP::MakeFullCollisionMatrix();
    HP::SetCollides(matrix, 2, 7, false);
    world.SetCollisionMatrix(matrix);
    CHECK(world.GetCollisionMatrix() == matrix);
    CHECK(world.Step(STEP));
}

TEST_CASE("Collision matrix - ShouldCollide needs both rows, SetCollides writes both")
{
    CollisionMatrix matrix = HP::MakeFullCollisionMatrix();
    for (uint32_t a = 0; a < HP::PHYSICS_LAYER_COUNT; ++a)
        for (uint32_t b = 0; b < HP::PHYSICS_LAYER_COUNT; ++b)
            CHECK(HP::ShouldCollide(matrix, a, b));

    HP::SetCollides(matrix, 3, 5, false);
    CHECK_FALSE(HP::ShouldCollide(matrix, 3, 5));
    CHECK_FALSE(HP::ShouldCollide(matrix, 5, 3));
    CHECK(HP::ShouldCollide(matrix, 3, 3));
    CHECK(HP::ShouldCollide(matrix, 5, 4));
    CHECK(((matrix[3] >> 5) & 1u) == 0);
    CHECK(((matrix[5] >> 3) & 1u) == 0);

    HP::SetCollides(matrix, 5, 3, true);
    CHECK(HP::ShouldCollide(matrix, 3, 5));

    // A one-sided entry never makes a pair collide.
    matrix[1] = static_cast<uint16_t>(matrix[1] & ~(1u << 9));
    CHECK_FALSE(HP::ShouldCollide(matrix, 1, 9));
    CHECK_FALSE(HP::ShouldCollide(matrix, 9, 1));

    // Layers out of range never collide and are never written.
    const CollisionMatrix before = matrix;
    CHECK_FALSE(HP::ShouldCollide(matrix, 0, HP::PHYSICS_LAYER_COUNT));
    CHECK_FALSE(HP::ShouldCollide(matrix, 99, 0));
    HP::SetCollides(matrix, 0, HP::PHYSICS_LAYER_COUNT, false);
    CHECK(matrix == before);
}

TEST_CASE("PhysicsWorld - an empty world steps on the calling thread and on a pool")
{
    for (const int32_t threads : { 0, 2 })
    {
        CAPTURE(threads);
        PhysicsWorldDesc desc;
        desc.WorkerThreads = threads;
        PhysicsWorld world;
        REQUIRE(world.Init(desc));
        CHECK(world.GetWorkerThreadCount() == threads);
        for (int step = 0; step < 60; ++step)
            REQUIRE(world.Step(STEP));
        CHECK(world.GetBodyCount() == 0);
    }
}
