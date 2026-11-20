#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include <limits>
#include <optional>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::ContactEvent;
using HP::ContactEventType;
using HP::MotionType;
using HP::PhysicsWorld;
using HP::PhysicsWorldDesc;
using HP::ShapeType;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    struct World
    {
        PhysicsWorld              Physics;
        std::vector<ContactEvent> Events;

        explicit World(int32_t workerThreads = 0, const HP::CollisionMatrix& matrix = HP::MakeFullCollisionMatrix())
        {
            PhysicsWorldDesc desc;
            desc.WorkerThreads = workerThreads;
            desc.Collisions    = matrix;
            REQUIRE(Physics.Init(desc));
        }

        // Steps, draining every step's events into Events.
        void Steps(int count)
        {
            for (int step = 0; step < count; ++step)
            {
                REQUIRE(Physics.Step(STEP));
                Physics.DrainContactEvents(Events);
            }
        }

        // A static floor 10 x 10 m whose top face is at z = 0.5.
        BodyHandle Floor(uint64_t userData, uint32_t layer = 0)
        {
            BodyDesc desc;
            desc.Motion            = MotionType::Static;
            desc.Shape.HalfExtents = HM::Vector3(5.0f, 5.0f, 0.5f);
            desc.Layer             = layer;
            desc.UserData          = userData;
            return Physics.CreateBody(desc);
        }

        BodyHandle Ball(uint64_t userData, const HM::Vector3& position, uint32_t layer = 0)
        {
            BodyDesc desc;
            desc.Shape.Type    = ShapeType::Sphere;
            desc.Pose.Position = position;
            desc.Layer         = layer;
            desc.UserData      = userData;
            return Physics.CreateBody(desc);
        }

        // A static box of half extent 1 that only detects what passes through it.
        BodyHandle Sensor(uint64_t userData, const HM::Vector3& position)
        {
            BodyDesc desc;
            desc.Motion            = MotionType::Static;
            desc.IsSensor          = true;
            desc.Shape.HalfExtents = HM::Vector3(1.0f, 1.0f, 1.0f);
            desc.Pose.Position     = position;
            desc.UserData          = userData;
            return Physics.CreateBody(desc);
        }

        size_t Count(ContactEventType type) const
        {
            size_t count = 0;
            for (const ContactEvent& event : Events)
                count += event.Type == type ? 1 : 0;
            return count;
        }
    };
}

TEST_CASE("Raycasts - the nearest body, its point, normal, distance and user data")
{
    World            world;
    const BodyHandle floor = world.Floor(1);
    BodyDesc         desc;
    desc.Motion        = MotionType::Static;
    desc.Pose.Position = HM::Vector3(0.0f, 0.0f, 3.0f);
    desc.Layer         = 1;
    desc.UserData      = 7;
    const BodyHandle box    = world.Physics.CreateBody(desc);
    const BodyHandle sensor = world.Sensor(9, HM::Vector3(0.0f, 0.0f, 6.0f));

    const HM::Vector3 origin(0.0f, 0.0f, 10.0f);
    const HM::Vector3 down(0.0f, 0.0f, -1.0f);

    // The sensor (top at 7) is skipped; the box's top is at 3.5.
    std::optional<HP::RayHit> hit = world.Physics.CastRay(origin, down, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == box);
    CHECK(hit->UserData == 7);
    CHECK(hit->Distance == doctest::Approx(6.5f));
    CHECK(hit->Point.z() == doctest::Approx(3.5f));
    CHECK(hit->Normal.z() == doctest::Approx(1.0f));

    // Any length of direction is the same ray.
    hit = world.Physics.CastRay(origin, down * 4.0f, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Distance == doctest::Approx(6.5f));

    // Asked for, the sensor is the nearest.
    hit = world.Physics.CastRay(origin, down, 100.0f, HP::ALL_LAYERS_MASK, true);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == sensor);
    CHECK(hit->Distance == doctest::Approx(3.0f));

    // Leaving the box's layer out of the mask reaches the floor.
    hit = world.Physics.CastRay(origin, down, 100.0f, static_cast<uint16_t>(HP::ALL_LAYERS_MASK & ~(1u << 1)));
    REQUIRE(hit.has_value());
    CHECK(hit->Body == floor);
    CHECK(hit->UserData == 1);
    CHECK(hit->Distance == doctest::Approx(9.5f));

    // A ray from the side hits the box's side with its normal.
    hit = world.Physics.CastRay(HM::Vector3(-5.0f, 0.0f, 3.0f), HM::Vector3(1.0f, 0.0f, 0.0f), 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == box);
    CHECK(hit->Point.x() == doctest::Approx(-0.5f));
    CHECK(hit->Normal.x() == doctest::Approx(-1.0f));

    // Too short, nowhere near, or no ray at all: nothing.
    CHECK_FALSE(world.Physics.CastRay(origin, down, 6.0f).has_value());
    CHECK_FALSE(world.Physics.CastRay(origin, HM::Vector3(0.0f, 0.0f, 1.0f), 100.0f).has_value());
    CHECK_FALSE(world.Physics.CastRay(origin, HM::Vector3(0.0f, 0.0f, 0.0f), 100.0f).has_value());
    CHECK_FALSE(world.Physics.CastRay(origin, down, 0.0f).has_value());
    CHECK_FALSE(world.Physics.CastRay(origin, down, 100.0f, 0).has_value());
    PhysicsWorld stopped;
    CHECK_FALSE(stopped.CastRay(origin, down, 100.0f).has_value());
}

TEST_CASE("Contacts - a ball landing on a floor enters once and exits when destroyed")
{
    World            world;
    const BodyHandle floor = world.Floor(1);
    const BodyHandle ball  = world.Ball(2, HM::Vector3(0.0f, 0.0f, 2.0f));
    world.Steps(120);

    REQUIRE(world.Events.size() == 1);
    const ContactEvent& enter = world.Events[0];
    CHECK(enter.Type == ContactEventType::Enter);
    CHECK(enter.BodyA == floor);
    CHECK(enter.BodyB == ball);
    CHECK(enter.UserDataA == 1);
    CHECK(enter.UserDataB == 2);
    CHECK_FALSE(enter.IsSensor);
    CHECK(enter.Point.z() == doctest::Approx(0.5f).epsilon(0.05));
    CHECK(enter.Normal.z() == doctest::Approx(1.0f).epsilon(1e-3));

    // Destroyed between steps, the ball's Exit comes from the next step with its user data.
    world.Events.clear();
    world.Physics.DestroyBody(ball);
    world.Physics.DrainContactEvents(world.Events);
    CHECK(world.Events.empty());
    world.Steps(1);
    REQUIRE(world.Events.size() == 1);
    CHECK(world.Events[0].Type == ContactEventType::Exit);
    CHECK(world.Events[0].BodyA == floor);
    CHECK(world.Events[0].BodyB == ball);
    CHECK(world.Events[0].UserDataA == 1);
    CHECK(world.Events[0].UserDataB == 2);

    world.Events.clear();
    world.Steps(10);
    CHECK(world.Events.empty());
}

TEST_CASE("Contacts - a ball bounced off the floor exits once")
{
    World world;
    world.Floor(1);
    const BodyHandle ball = world.Ball(2, HM::Vector3(0.0f, 0.0f, 2.0f));
    world.Steps(120);
    REQUIRE(world.Count(ContactEventType::Enter) == 1);

    world.Physics.SetLinearVelocity(ball, HM::Vector3(0.0f, 0.0f, 5.0f));
    world.Steps(20);
    CHECK(world.Count(ContactEventType::Enter) == 1);
    REQUIRE(world.Count(ContactEventType::Exit) == 1);
    CHECK(world.Events.back().UserDataA == 1);
    CHECK(world.Events.back().UserDataB == 2);
}

TEST_CASE("Contacts - falling asleep is not an Exit, and a sleeping ball teleported away exits once")
{
    World world;
    world.Floor(1);
    const BodyHandle ball = world.Ball(2, HM::Vector3(0.0f, 0.0f, 2.0f));
    world.Steps(120);
    std::vector<BodyHandle> active;
    world.Physics.GetActiveBodies(active);
    REQUIRE(active.empty());
    REQUIRE(world.Events.size() == 1);

    world.Physics.SetPose(ball, HP::BodyPose{ HM::Vector3(0.0f, 0.0f, 50.0f), {} });
    world.Steps(3);
    REQUIRE(world.Events.size() == 2);
    CHECK(world.Events[1].Type == ContactEventType::Exit);
    CHECK(world.Events[1].UserDataB == 2);
}

TEST_CASE("Contacts - a body falling through a sensor enters and exits it")
{
    World world;
    world.Floor(1);
    world.Sensor(3, HM::Vector3(0.0f, 0.0f, 5.0f));
    world.Ball(2, HM::Vector3(0.0f, 0.0f, 10.0f));
    world.Steps(150);

    std::vector<ContactEvent> sensorEvents;
    for (const ContactEvent& event : world.Events)
        if (event.IsSensor)
            sensorEvents.push_back(event);
    REQUIRE(sensorEvents.size() == 2);
    CHECK(sensorEvents[0].Type == ContactEventType::Enter);
    CHECK(sensorEvents[1].Type == ContactEventType::Exit);
    for (const ContactEvent& event : sensorEvents)
    {
        CHECK(event.UserDataA == 2);
        CHECK(event.UserDataB == 3);
    }
    // And the floor stopped it, as an ordinary contact.
    CHECK(world.Events.size() == 3);
}

TEST_CASE("Contacts - layers the matrix keeps apart give no events")
{
    HP::CollisionMatrix matrix = HP::MakeFullCollisionMatrix();
    HP::SetCollides(matrix, 0, 1, false);
    World world(0, matrix);
    world.Floor(1, 0);
    world.Ball(2, HM::Vector3(0.0f, 0.0f, 2.0f), 1);
    world.Steps(60);
    CHECK(world.Events.empty());
}

TEST_CASE("Contacts - the drained order is the same with 0 and 4 worker threads")
{
    std::vector<ContactEvent> events[2];
    for (const int32_t threads : { 0, 4 })
    {
        CAPTURE(threads);
        World world(threads);
        world.Floor(1);
        world.Sensor(2, HM::Vector3(0.0f, 0.0f, 3.0f));
        uint64_t userData = 10;
        for (int x = -2; x <= 2; ++x)
            for (int y = -2; y <= 2; ++y)
                world.Ball(userData++, HM::Vector3(1.2f * static_cast<float>(x), 1.2f * static_cast<float>(y),
                                                   6.0f + 0.3f * static_cast<float>(x + y)));
        world.Steps(150);
        events[threads == 0 ? 0 : 1] = world.Events;
    }

    REQUIRE(events[0].size() > 25);
    REQUIRE(events[0].size() == events[1].size());
    for (size_t index = 0; index < events[0].size(); ++index)
    {
        CAPTURE(index);
        CHECK(events[0][index].Type == events[1][index].Type);
        CHECK(events[0][index].UserDataA == events[1][index].UserDataA);
        CHECK(events[0][index].UserDataB == events[1][index].UserDataB);
        CHECK(events[0][index].IsSensor == events[1][index].IsSensor);
    }
}

namespace
{
    HP::ShapeDesc Sphere(float radius)
    {
        HP::ShapeDesc shape;
        shape.Type   = ShapeType::Sphere;
        shape.Radius = radius;
        return shape;
    }

    HP::BodyPose At(const HM::Vector3& position)
    {
        HP::BodyPose pose;
        pose.Position = position;
        return pose;
    }

    BodyHandle StaticBox(World& world, uint64_t userData, const HM::Vector3& position, uint32_t layer = 0)
    {
        BodyDesc desc;
        desc.Motion        = MotionType::Static;
        desc.Pose.Position = position;
        desc.Layer         = layer;
        desc.UserData      = userData;
        return world.Physics.CreateBody(desc);
    }

    std::vector<uint64_t> UserData(const std::vector<HP::OverlapHit>& hits)
    {
        std::vector<uint64_t> data;
        for (const HP::OverlapHit& hit : hits)
            data.push_back(hit.UserData);
        return data;
    }
}

TEST_CASE("Shape casts - a sphere hits a box where they first touch, with the box's face normal")
{
    World            world;
    const BodyHandle box = StaticBox(world, 7, HM::Vector3(0.0f, 0.0f, 3.0f));

    // The sphere's centre stops a radius short of the box's face at x = -0.5.
    const HP::BodyPose                start = At(HM::Vector3(-5.0f, 0.0f, 3.0f));
    const HM::Vector3                 along(2.0f, 0.0f, 0.0f);
    const std::optional<HP::ShapeHit> hit = world.Physics.CastShape(Sphere(0.5f), start, along, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == box);
    CHECK(hit->UserData == 7);
    CHECK(hit->Distance == doctest::Approx(4.0f).epsilon(1e-3));
    CHECK(hit->Point.x() == doctest::Approx(-0.5f).epsilon(1e-3));
    CHECK(hit->Normal.x() == doctest::Approx(-1.0f).epsilon(1e-3));

    // Not far enough to reach it.
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.5f), start, along, 3.9f).has_value());
}

TEST_CASE("Shape casts - a gap a ray passes through blocks a wider sphere")
{
    World world;
    // Two boxes 0.2 m apart along y.
    StaticBox(world, 1, HM::Vector3(0.0f, 0.6f, 3.0f));
    StaticBox(world, 2, HM::Vector3(0.0f, -0.6f, 3.0f));
    const HM::Vector3 origin(-5.0f, 0.0f, 3.0f);
    const HM::Vector3 along(1.0f, 0.0f, 0.0f);

    CHECK_FALSE(world.Physics.CastRay(origin, along, 10.0f).has_value());
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.05f), At(origin), along, 10.0f).has_value());
    const std::optional<HP::ShapeHit> hit = world.Physics.CastShape(Sphere(0.3f), At(origin), along, 10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Distance < 4.5f);
}

TEST_CASE("Shape casts - boxes and capsules, and a cast starting in contact")
{
    World             world;
    const BodyHandle  floor = world.Floor(1); // its top at z = 0.5
    const HM::Vector3 down(0.0f, 0.0f, -1.0f);

    // A 1 m box from z = 5: its bottom, at 4.5, travels 4.
    HP::ShapeDesc box;
    box.HalfExtents                 = HM::Vector3(0.5f, 0.5f, 0.5f);
    std::optional<HP::ShapeHit> hit = world.Physics.CastShape(box, At(HM::Vector3(0.0f, 0.0f, 5.0f)), down, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == floor);
    CHECK(hit->Distance == doctest::Approx(4.0f).epsilon(1e-3));
    CHECK(hit->Normal.z() == doctest::Approx(1.0f).epsilon(1e-3));

    // Turned a quarter about x, the box is still 1 m tall.
    HP::BodyPose turned = At(HM::Vector3(0.0f, 0.0f, 5.0f));
    turned.Rotation     = HM::Quaternion::FromEuler(HM::Vector3(90.0f, 0.0f, 0.0f));
    hit                 = world.Physics.CastShape(box, turned, down, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Distance == doctest::Approx(4.0f).epsilon(1e-3));

    // A capsule along +Z, 2 m end to end: its bottom, at 4, travels 3.5.
    HP::ShapeDesc capsule;
    capsule.Type       = ShapeType::Capsule;
    capsule.Radius     = 0.5f;
    capsule.HalfHeight = 0.5f;
    hit                = world.Physics.CastShape(capsule, At(HM::Vector3(0.0f, 0.0f, 5.0f)), down, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Distance == doctest::Approx(3.5f).epsilon(1e-3));

    // A sphere sunk into the floor hits it where it starts, whichever way it is cast.
    for (const HM::Vector3& direction : { down, HM::Vector3(0.0f, 0.0f, 1.0f) })
    {
        hit = world.Physics.CastShape(Sphere(0.5f), At(HM::Vector3(0.0f, 0.0f, 0.8f)), direction, 10.0f);
        REQUIRE(hit.has_value());
        CHECK(hit->Body == floor);
        CHECK(hit->Distance == doctest::Approx(0.0f));
    }
}

TEST_CASE("Shape casts - the layer mask and sensors behave as a ray's do")
{
    World             world;
    const BodyHandle  floor  = world.Floor(1);
    const BodyHandle  box    = StaticBox(world, 7, HM::Vector3(0.0f, 0.0f, 3.0f), 1);
    const BodyHandle  sensor = world.Sensor(9, HM::Vector3(0.0f, 0.0f, 6.0f));
    const HM::Vector3 origin(0.0f, 0.0f, 10.0f);
    const HM::Vector3 down(0.0f, 0.0f, -1.0f);
    const uint16_t    noBoxLayer = static_cast<uint16_t>(HP::ALL_LAYERS_MASK & ~(1u << 1));

    std::optional<HP::ShapeHit> hit = world.Physics.CastShape(Sphere(0.25f), At(origin), down, 100.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == box);
    hit = world.Physics.CastShape(Sphere(0.25f), At(origin), down, 100.0f, HP::ALL_LAYERS_MASK, true);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == sensor);
    hit = world.Physics.CastShape(Sphere(0.25f), At(origin), down, 100.0f, noBoxLayer);
    REQUIRE(hit.has_value());
    CHECK(hit->Body == floor);
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.25f), At(origin), down, 100.0f, 0).has_value());
}

TEST_CASE("Overlaps - exactly the bodies inside, sorted and once each, a sensor only when asked")
{
    World world;
    world.Floor(1); // its top at z = 0.5
    world.Ball(5, HM::Vector3(2.0f, 0.0f, 1.0f));
    world.Ball(3, HM::Vector3(-2.0f, 0.0f, 1.0f));
    world.Ball(4, HM::Vector3(4.0f, 4.0f, 8.0f));
    world.Sensor(9, HM::Vector3(0.0f, 0.0f, 1.5f));
    StaticBox(world, 2, HM::Vector3(0.0f, 0.0f, 1.0f), 1);

    HP::ShapeDesc box;
    box.HalfExtents = HM::Vector3(3.0f, 1.0f, 1.0f);
    std::vector<HP::OverlapHit> hits(3); // replaced, not appended to
    world.Physics.Overlap(box, At(HM::Vector3(0.0f, 0.0f, 1.0f)), hits);
    CHECK(UserData(hits) == std::vector<uint64_t>{ 1, 2, 3, 5 });
    world.Physics.Overlap(box, At(HM::Vector3(0.0f, 0.0f, 1.0f)), hits, HP::ALL_LAYERS_MASK, true);
    CHECK(UserData(hits) == std::vector<uint64_t>{ 1, 2, 3, 5, 9 });
    world.Physics.Overlap(box, At(HM::Vector3(0.0f, 0.0f, 1.0f)), hits, static_cast<uint16_t>(1u << 0));
    CHECK(UserData(hits) == std::vector<uint64_t>{ 1, 3, 5 });

    // A small sphere in the air touches nothing; one around the far ball touches only it.
    world.Physics.Overlap(Sphere(0.1f), At(HM::Vector3(0.0f, 3.0f, 3.0f)), hits);
    CHECK(hits.empty());
    world.Physics.Overlap(Sphere(0.1f), At(HM::Vector3(4.0f, 4.0f, 8.0f)), hits);
    CHECK(UserData(hits) == std::vector<uint64_t>{ 4 });
}

TEST_CASE("Shape queries - a height field and bad input give nothing")
{
    World world;
    world.Floor(1);
    const HM::Vector3           origin(0.0f, 0.0f, 3.0f);
    const HM::Vector3           down(0.0f, 0.0f, -1.0f);
    const HP::BodyPose          sunk = At(HM::Vector3(0.0f, 0.0f, 0.8f));
    const float                 nan  = std::numeric_limits<float>::quiet_NaN();
    std::vector<HP::OverlapHit> hits;
    const auto refused = [&](const HP::ShapeDesc& shape, const HP::BodyPose& pose) {
        world.Physics.Overlap(shape, pose, hits);
        return !world.Physics.CastShape(shape, pose, down, 100.0f).has_value() && hits.empty();
    };
    // Sunk into the floor, a good sphere is found by both.
    CHECK_FALSE(refused(Sphere(0.5f), sunk));

    HP::ShapeDesc field;
    field.Type        = ShapeType::HeightField;
    field.SampleCount = 4;
    field.Heights.assign(16, 0.0f);
    CHECK(refused(field, sunk));
    CHECK(refused(Sphere(0.0f), sunk));
    CHECK(refused(Sphere(-1.0f), sunk));
    HP::ShapeDesc flat;
    flat.HalfExtents = HM::Vector3(1.0f, 0.0f, 1.0f);
    CHECK(refused(flat, sunk));
    HP::ShapeDesc unscaled = Sphere(0.5f);
    unscaled.Scale         = HM::Vector3(1.0f, 0.0f, 1.0f);
    CHECK(refused(unscaled, sunk));
    CHECK(refused(Sphere(0.5f), At(HM::Vector3(0.0f, nan, 0.8f))));

    CHECK_FALSE(world.Physics.CastShape(Sphere(0.5f), At(origin), HM::Vector3(0.0f, 0.0f, 0.0f), 100.0f).has_value());
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.5f), At(origin), HM::Vector3(0.0f, nan, -1.0f), 100.0f).has_value());
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.5f), At(origin), down, 0.0f).has_value());
    CHECK_FALSE(world.Physics.CastShape(Sphere(0.5f), At(origin), down, nan).has_value());
    PhysicsWorld stopped;
    CHECK_FALSE(stopped.CastShape(Sphere(0.5f), At(origin), down, 100.0f).has_value());
    hits.resize(2);
    stopped.Overlap(Sphere(0.5f), At(origin), hits);
    CHECK(hits.empty());
}
