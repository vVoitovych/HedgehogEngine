#include "doctest/doctest/doctest.h"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <limits>
#include <vector>

using HP::BodyDesc;
using HP::BodyHandle;
using HP::MotionType;
using HP::PhysicsWorld;
using HP::ShapeType;

namespace
{
    constexpr float    STEP    = 1.0f / 60.0f;
    constexpr uint32_t SAMPLES = 33;

    // A static height field of SAMPLES x SAMPLES samples cellSize apart, height(column, row) each.
    template <typename Height>
    BodyDesc MakeHeightField(float cellSize, Height height)
    {
        BodyDesc desc;
        desc.Motion            = MotionType::Static;
        desc.Shape.Type        = ShapeType::HeightField;
        desc.Shape.SampleCount = SAMPLES;
        desc.Shape.CellSize    = cellSize;
        desc.Shape.Heights.resize(SAMPLES * SAMPLES);
        for (uint32_t row = 0; row < SAMPLES; ++row)
            for (uint32_t column = 0; column < SAMPLES; ++column)
                desc.Shape.Heights[row * SAMPLES + column] = height(column, row);
        return desc;
    }

    struct World
    {
        PhysicsWorld Physics;

        World() { REQUIRE(Physics.Init(HP::PhysicsWorldDesc{})); }

        void Steps(int count)
        {
            for (int step = 0; step < count; ++step)
                REQUIRE(Physics.Step(STEP));
        }

        BodyHandle Drop(ShapeType type, const HM::Vector3& position)
        {
            BodyDesc desc;
            desc.Shape.Type    = type;
            desc.Pose.Position = position;
            return Physics.CreateBody(desc);
        }

        float HeightBelow(float x, float y)
        {
            const std::optional<HP::RayHit> hit = Physics.CastRay(HM::Vector3(x, y, 100.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 200.0f);
            REQUIRE(hit);
            return hit->Point.z();
        }
    };
}

TEST_CASE("Height field - a ball and a box come to rest on a flat field at its height")
{
    World world;
    REQUIRE(world.Physics.CreateBody(MakeHeightField(1.0f, [](uint32_t, uint32_t) { return 2.0f; })).IsSet());
    const BodyHandle ball = world.Drop(ShapeType::Sphere, HM::Vector3(10.3f, 20.6f, 6.0f));
    const BodyHandle box  = world.Drop(ShapeType::Box, HM::Vector3(5.5f, 5.5f, 6.0f));
    world.Steps(240);

    CHECK(world.Physics.GetPose(ball).Position.z() == doctest::Approx(2.5f).epsilon(0.01));
    CHECK(world.Physics.GetPose(box).Position.z() == doctest::Approx(2.5f).epsilon(0.01));
    CHECK(world.Physics.GetPose(ball).Position.x() == doctest::Approx(10.3f).epsilon(0.01));
}

TEST_CASE("Height field - a box rests on a slope where the slope's height says")
{
    constexpr float slope = 0.2f; // gentler than the friction, so the box does not slide
    World           world;
    REQUIRE(world.Physics.CreateBody(MakeHeightField(1.0f, [](uint32_t column, uint32_t) { return slope * column; })).IsSet());
    const BodyHandle box = world.Drop(ShapeType::Box, HM::Vector3(16.0f, 16.0f, 6.0f));
    world.Steps(300);

    // A unit box lying on the slope: its centre half a metre along the surface normal.
    const HP::BodyPose pose     = world.Physics.GetPose(box);
    const float        angle    = std::atan(slope);
    const float        expected = slope * pose.Position.x() + 0.5f * std::cos(angle) - slope * 0.5f * std::sin(angle);
    CHECK(pose.Position.z() == doctest::Approx(expected).epsilon(0.01));
    CHECK(world.Physics.GetLinearVelocity(box).LengthSlow() < 0.01f);
}

TEST_CASE("Height field - rays meet the cells' triangles split from (c, r) to (c + 1, r + 1)")
{
    World world;
    // One raised sample at column 1, row 0.
    BodyDesc desc = MakeHeightField(1.0f, [](uint32_t column, uint32_t row) { return column == 1 && row == 0 ? 4.0f : 0.0f; });
    desc.UserData = 7;
    REQUIRE(world.Physics.CreateBody(desc).IsSet());

    // On the (0, 0)-(1, 0)-(1, 1) triangle the height is 4 at (1, 0) and 0 on the diagonal.
    CHECK(world.HeightBelow(0.75f, 0.25f) == doctest::Approx(2.0f).epsilon(0.001));
    CHECK(world.HeightBelow(0.25f, 0.75f) == doctest::Approx(0.0f).scale(1.0f).epsilon(0.001));
    CHECK(world.HeightBelow(10.5f, 20.5f) == doctest::Approx(0.0f).scale(1.0f).epsilon(0.001));

    const std::optional<HP::RayHit> hit =
        world.Physics.CastRay(HM::Vector3(0.75f, 0.25f, 10.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 20.0f);
    REQUIRE(hit);
    CHECK(hit->UserData == 7);
    CHECK(hit->Normal.z() > 0.0f);
    CHECK(hit->Normal.y() > 0.0f); // the raised sample leans the triangle towards +y
}

TEST_CASE("Height field - its body's pose, scale and cell size place it")
{
    World    world;
    BodyDesc desc          = MakeHeightField(0.5f, [](uint32_t column, uint32_t row) { return 0.1f * column + 0.05f * row; });
    desc.Pose.Position     = HM::Vector3(100.0f, -50.0f, 3.0f);
    desc.Shape.Scale       = HM::Vector3(2.0f, 3.0f, 2.0f);
    REQUIRE(world.Physics.CreateBody(desc).IsSet());

    // Column 10, row 4 lies at (10 * 0.5 * 2, 4 * 0.5 * 3) from the origin, its height doubled.
    CHECK(world.HeightBelow(110.0f, -44.0f) == doctest::Approx(3.0f + 2.0f * (0.1f * 10 + 0.05f * 4)).epsilon(0.001));
    // Past the field's far corner (32 * 0.5 * 2, 32 * 0.5 * 3) nothing is hit.
    CHECK_FALSE(world.Physics.CastRay(HM::Vector3(133.0f, -1.0f, 100.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 200.0f));
}

TEST_CASE("Height field - a desc that cannot make one is refused with one error")
{
    World world;
    std::vector<BodyDesc> bad(5, MakeHeightField(1.0f, [](uint32_t, uint32_t) { return 0.0f; }));
    bad[0].Motion            = MotionType::Dynamic;
    bad[1].Shape.SampleCount = 3;
    bad[1].Shape.Heights.resize(9);
    bad[2].Shape.Heights.pop_back();
    bad[3].Shape.Heights[5]  = std::numeric_limits<float>::quiet_NaN();
    bad[4].Shape.CellSize    = 0.0f;

    for (size_t index = 0; index < bad.size(); ++index)
    {
        CAPTURE(index);
        LogCapture log;
        CHECK_FALSE(world.Physics.CreateBody(bad[index]).IsSet());
        CHECK(log.Lines("[ERROR]").size() == 1);
    }
    CHECK(world.Physics.GetBodyCount() == 0);
}
