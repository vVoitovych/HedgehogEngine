#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "HedgehogMath/api/Quaternion.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <limits>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::CharacterDesc;
using HP::CharacterGroundState;
using HP::CharacterHandle;
using HP::MotionType;
using HP::PhysicsWorld;

namespace
{
    constexpr float    STEP = 1.0f / 60.0f;
    const HM::Vector3  GRAVITY(0.0f, 0.0f, -9.81f);
    const HM::Vector3  STILL(0.0f, 0.0f, 0.0f);
    constexpr uint64_t CHARACTER_DATA = 42;
    constexpr float    REST_HEIGHT    = 0.5f + 0.3f + 0.6f; // the floor's top, the radius and half height

    struct World
    {
        PhysicsWorld Physics;

        explicit World(int32_t workerThreads = 0)
        {
            HP::PhysicsWorldDesc desc;
            desc.WorkerThreads = workerThreads;
            REQUIRE(Physics.Init(desc));
        }

        // A static box: half extents, centre, turned about +Y by pitch degrees, with user data.
        BodyHandle Box(const HM::Vector3& halfExtents, const HM::Vector3& center, float pitch = 0.0f,
                       MotionType motion = MotionType::Static, uint64_t userData = 0, float mass = 1.0f)
        {
            BodyDesc desc;
            desc.Motion            = motion;
            desc.Shape.HalfExtents = halfExtents;
            desc.Pose.Position     = center;
            desc.Pose.Rotation     = HM::Quaternion::FromEuler(HM::Vector3(0.0f, pitch, 0.0f));
            desc.UserData          = userData;
            desc.Mass              = mass;
            return Physics.CreateBody(desc);
        }

        // A 20 x 20 m floor whose top face is at z = 0.5.
        BodyHandle Floor() { return Box(HM::Vector3(10.0f, 10.0f, 0.5f), STILL, 0.0f, MotionType::Static, 7); }

        CharacterHandle Character(const HM::Vector3& position)
        {
            CharacterDesc desc;
            desc.Pose.Position = position;
            desc.UserData      = CHARACTER_DATA;
            return Physics.CreateCharacter(desc);
        }

        // Moves the character at velocity and steps the world, frames times.
        void Walk(CharacterHandle character, const HM::Vector3& velocity, int frames)
        {
            for (int frame = 0; frame < frames; ++frame)
            {
                Physics.MoveCharacter(character, velocity, GRAVITY, STEP);
                REQUIRE(Physics.Step(STEP));
            }
        }

        HM::Vector3 Position(CharacterHandle character) const { return Physics.GetCharacterState(character).Position; }
    };
}

TEST_CASE("Character - dropped onto a floor it falls, then stands on it with the normal up")
{
    World                 world;
    (void)world.Floor();
    const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, 3.0f));
    REQUIRE(character.IsSet());
    CHECK(world.Physics.GetCharacterCount() == 1);

    world.Walk(character, STILL, 5);
    CHECK(world.Physics.GetCharacterState(character).Ground == CharacterGroundState::InAir);
    CHECK(world.Position(character).z() < 3.0f);

    world.Walk(character, STILL, 120);
    const HP::CharacterState state = world.Physics.GetCharacterState(character);
    CHECK(state.Ground == CharacterGroundState::OnGround);
    CHECK(state.GroundNormal.z() == doctest::Approx(1.0f).epsilon(1e-3));
    CHECK(state.GroundUserData == 7);
    CHECK(state.Position.z() == doctest::Approx(REST_HEIGHT).epsilon(0.01 / REST_HEIGHT));
}

TEST_CASE("Character - a wall stops it; it walks up a 30 degree slope but not a 60 degree one")
{
    {
        World world;
        (void)world.Floor();
        (void)world.Box(HM::Vector3(0.25f, 5.0f, 2.0f), HM::Vector3(3.0f, 0.0f, 2.0f));
        const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT));
        world.Walk(character, HM::Vector3(3.0f, 0.0f, 0.0f), 120);
        CHECK(world.Position(character).x() < 3.0f - 0.25f - 0.3f + 0.05f);
        CHECK(world.Position(character).x() > 2.0f);
    }
    // A long ramp through the floor, rising along +x from x = 0.
    for (const float slope : { 30.0f, 60.0f })
    {
        CAPTURE(slope);
        World       world;
        (void)world.Floor();
        const float radians = slope * 3.14159265f / 180.0f;
        const float length  = 8.0f;
        (void)world.Box(HM::Vector3(length, 5.0f, 0.1f),
                        HM::Vector3(length * std::cos(radians), 0.0f, 0.5f + length * std::sin(radians)), -slope);
        const CharacterHandle character = world.Character(HM::Vector3(-1.0f, 0.0f, REST_HEIGHT));
        world.Walk(character, HM::Vector3(2.0f, 0.0f, 0.0f), 180);
        const HP::CharacterState state = world.Physics.GetCharacterState(character);
        if (slope < 50.0f)
        {
            CHECK(state.Position.z() > REST_HEIGHT + 1.5f); // walked well up it
            CHECK(state.Ground == CharacterGroundState::OnGround);
        }
        else
        {
            CHECK(state.Position.z() < REST_HEIGHT + 0.5f);
            CHECK(state.Ground != CharacterGroundState::InAir);
        }
    }
    {
        // Set on the steep slope, it is on steep ground.
        World       world;
        const float radians = 60.0f * 3.14159265f / 180.0f;
        (void)world.Box(HM::Vector3(8.0f, 5.0f, 0.1f), STILL, -60.0f);
        const HM::Vector3     normal(-std::sin(radians), 0.0f, std::cos(radians));
        const CharacterHandle character =
            world.Character(HM::Vector3(normal.x() * 0.45f, 0.0f, 0.6f * std::sin(radians) + normal.z() * 0.45f));
        world.Walk(character, STILL, 2);
        CHECK(world.Physics.GetCharacterState(character).Ground == CharacterGroundState::OnSteepGround);
    }
}

TEST_CASE("Character - it climbs a 0.2 m step but not a 0.5 m one")
{
    for (const float height : { 0.2f, 0.5f })
    {
        CAPTURE(height);
        World world;
        (void)world.Floor();
        (void)world.Box(HM::Vector3(2.0f, 5.0f, height * 0.5f), HM::Vector3(3.0f, 0.0f, 0.5f + height * 0.5f));
        const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT));
        world.Walk(character, HM::Vector3(2.0f, 0.0f, 0.0f), 90);
        const HM::Vector3 position = world.Position(character);
        if (height < 0.3f)
        {
            CHECK(position.x() > 1.5f);
            CHECK(position.z() == doctest::Approx(REST_HEIGHT + height).epsilon(0.02));
        }
        else
        {
            CHECK(position.x() < 1.0f);
            CHECK(position.z() == doctest::Approx(REST_HEIGHT).epsilon(0.02));
        }
    }
}

TEST_CASE("Character - it pushes a 1 kg box and is carried by a kinematic platform")
{
    {
        World            world;
        (void)world.Floor();
        const BodyHandle box =
            world.Box(HM::Vector3(0.25f, 0.25f, 0.25f), HM::Vector3(1.5f, 0.0f, 0.75f), 0.0f, MotionType::Dynamic);
        const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT));
        world.Walk(character, HM::Vector3(2.0f, 0.0f, 0.0f), 90);
        CHECK(world.Physics.GetPose(box).Position.x() > 2.0f);
    }
    {
        World            world;
        const BodyHandle platform = world.Box(HM::Vector3(3.0f, 3.0f, 0.25f), HM::Vector3(0.0f, 0.0f, 0.25f), 0.0f,
                                              MotionType::Kinematic, 99);
        world.Physics.SetLinearVelocity(platform, HM::Vector3(1.0f, 0.0f, 0.0f));
        const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, 0.5f + 0.9f));
        world.Walk(character, STILL, 30);
        const HP::CharacterState state = world.Physics.GetCharacterState(character);
        CHECK(state.Ground == CharacterGroundState::OnGround);
        CHECK(state.GroundUserData == 99);
        CHECK(state.GroundVelocity.x() == doctest::Approx(1.0f).epsilon(1e-3));
        CHECK(state.Position.x() > 0.3f); // carried along
    }
}

TEST_CASE("Character - rays hit its inner body, and a sensor it walks through reports it")
{
    World world;
    (void)world.Floor();
    BodyDesc sensor;
    sensor.Motion            = MotionType::Static;
    sensor.IsSensor          = true;
    sensor.Shape.HalfExtents = HM::Vector3(0.5f, 2.0f, 1.5f);
    sensor.Pose.Position     = HM::Vector3(2.0f, 0.0f, 1.5f);
    sensor.UserData          = 5;
    REQUIRE(world.Physics.CreateBody(sensor).IsSet());
    const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, REST_HEIGHT));
    REQUIRE(world.Physics.Step(STEP));

    const auto hit = world.Physics.CastRay(HM::Vector3(0.0f, 0.0f, 5.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 10.0f);
    REQUIRE(hit.has_value());
    CHECK(hit->UserData == CHARACTER_DATA);
    CHECK(hit->Point.z() == doctest::Approx(REST_HEIGHT + 0.9f).epsilon(0.01));

    std::vector<HP::ContactEvent> events;
    for (int frame = 0; frame < 150; ++frame)
    {
        world.Physics.MoveCharacter(character, HM::Vector3(2.0f, 0.0f, 0.0f), GRAVITY, STEP);
        REQUIRE(world.Physics.Step(STEP));
        world.Physics.DrainContactEvents(events);
    }
    int enters = 0;
    int exits  = 0;
    for (const HP::ContactEvent& event : events)
    {
        const bool ours = (event.UserDataA == 5 && event.UserDataB == CHARACTER_DATA) ||
                          (event.UserDataA == CHARACTER_DATA && event.UserDataB == 5);
        if (!ours)
            continue;
        CHECK(event.IsSensor);
        (event.Type == HP::ContactEventType::Enter ? enters : exits)++;
    }
    CHECK(enters == 1);
    CHECK(exits == 1);
}

TEST_CASE("Character - bad descs are refused with one error each, and stale handles do nothing")
{
    World      world;
    LogCapture log;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CharacterDesc bad[6];
    bad[0].Radius             = 0.0f;
    bad[1].Pose.Position      = HM::Vector3(nan, 0.0f, 0.0f);
    bad[2].Layer              = HP::PHYSICS_LAYER_COUNT;
    bad[3].Mass               = 0.0f;
    bad[4].MaxSlopeDegrees    = 95.0f;
    bad[5].StepHeight         = -0.1f;
    for (const CharacterDesc& desc : bad)
        CHECK_FALSE(world.Physics.CreateCharacter(desc).IsSet());
    CHECK(log.Lines("[Physics] A character cannot be made").size() == 6);
    CHECK(world.Physics.GetCharacterCount() == 0);

    const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, 2.0f));
    world.Physics.DestroyCharacter(character);
    CHECK_FALSE(world.Physics.IsValid(character));
    world.Physics.MoveCharacter(character, HM::Vector3(1.0f, 0.0f, 0.0f), GRAVITY, STEP);
    world.Physics.SetCharacterPose(character, {});
    CHECK(world.Physics.GetCharacterState(character).Position.z() == 0.0f);

    // The slot is reused under a new generation; the old handle still reaches nothing.
    const CharacterHandle next = world.Character(HM::Vector3(0.0f, 0.0f, 2.0f));
    CHECK(next.Index == character.Index);
    CHECK_FALSE(world.Physics.IsValid(character));
    world.Physics.DestroyCharacter(character);
    CHECK(world.Physics.IsValid(next));

    world.Physics.DestroyAllBodies();
    CHECK(world.Physics.GetCharacterCount() == 0);

    PhysicsWorld stopped;
    CHECK_FALSE(stopped.CreateCharacter({}).IsSet());
}

TEST_CASE("Character - the same walk lands in the same place on 0 and 2 worker threads")
{
    HM::Vector3 results[2];
    for (const int32_t threads : { 0, 2 })
    {
        World world(threads);
        (void)world.Floor();
        (void)world.Box(HM::Vector3(2.0f, 5.0f, 0.1f), HM::Vector3(3.0f, 0.0f, 0.6f));
        const CharacterHandle character = world.Character(HM::Vector3(0.0f, 0.0f, 2.0f));
        world.Walk(character, HM::Vector3(2.0f, 0.5f, 0.0f), 120);
        results[threads == 0 ? 0 : 1] = world.Position(character);
    }
    CHECK(results[0].x() == results[1].x());
    CHECK(results[0].y() == results[1].y());
    CHECK(results[0].z() == results[1].z());
}
