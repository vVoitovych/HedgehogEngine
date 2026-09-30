#include "doctest/doctest/doctest.h"

#include "test_common.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Common.hpp"

#include <cmath>
#include <random>

namespace
{
    // The rotation TransformSystem builds from a TransformComponent's Euler angles.
    HM::Matrix4x4 TransformSystemRotation(const HM::Vector3& eulerDegrees)
    {
        return HM::Matrix4x4::GetRotationX(HM::ToRadians(eulerDegrees.x())) *
               HM::Matrix4x4::GetRotationY(HM::ToRadians(eulerDegrees.y())) *
               HM::Matrix4x4::GetRotationZ(HM::ToRadians(eulerDegrees.z()));
    }

    // q and -q are the same rotation.
    bool SameRotation(const HM::Quaternion& a, const HM::Quaternion& b, float epsilon = 1e-5f)
    {
        return std::abs(std::abs(HM::Dot(a, b)) - 1.0f) < epsilon;
    }

    bool NearlyEqual(const HM::Quaternion& a, const HM::Quaternion& b, float epsilon = 1e-5f)
    {
        return HedgehogTest::NearlyEqual(HM::Vector4(a.x(), a.y(), a.z(), a.w()),
                                         HM::Vector4(b.x(), b.y(), b.z(), b.w()), epsilon);
    }

    HM::Vector3 RandomEuler(std::mt19937& generator)
    {
        std::uniform_real_distribution<float> angle(-180.0f, 180.0f);
        const float x = angle(generator);
        const float y = angle(generator);
        const float z = angle(generator);
        return HM::Vector3(x, y, z);
    }
}

TEST_CASE("Quaternion - default is the identity, and the identity changes nothing")
{
    const HM::Quaternion identity;
    CHECK(identity == HM::Quaternion::Identity());
    CHECK(identity == HM::Quaternion(0.0f, 0.0f, 0.0f, 1.0f));
    CHECK(HedgehogTest::NearlyEqual(identity.ToMatrix(), HM::Matrix4x4::GetIdentity()));
    CHECK(HedgehogTest::NearlyEqual(identity * HM::Vector3(1.0f, 2.0f, 3.0f), HM::Vector3(1.0f, 2.0f, 3.0f)));
    CHECK(HedgehogTest::NearlyEqual(identity.ToEuler(), HM::Vector3(0.0f, 0.0f, 0.0f)));
}

TEST_CASE("Quaternion - FromEuler matches TransformSystem's Rx * Ry * Rz for 1000 random angles")
{
    std::mt19937 generator(20260930u);
    for (int i = 0; i < 1000; ++i)
    {
        const HM::Vector3 euler = RandomEuler(generator);
        INFO("euler = ", euler.x(), ", ", euler.y(), ", ", euler.z());
        REQUIRE(HedgehogTest::NearlyEqual(HM::Quaternion::FromEuler(euler).ToMatrix(),
                                          TransformSystemRotation(euler)));
    }
}

TEST_CASE("Quaternion - FromEuler(ToEuler(q)) reproduces q up to sign for 1000 random rotations")
{
    std::mt19937 generator(7u);
    for (int i = 0; i < 1000; ++i)
    {
        const HM::Quaternion q     = HM::Quaternion::FromEuler(RandomEuler(generator));
        const HM::Vector3    euler = q.ToEuler();
        INFO("euler = ", euler.x(), ", ", euler.y(), ", ", euler.z());
        CHECK(euler.y() >= -90.0f);
        CHECK(euler.y() <= 90.0f);
        REQUIRE(SameRotation(HM::Quaternion::FromEuler(euler), q));
    }
}

TEST_CASE("Quaternion - ToEuler at gimbal lock still gives the same rotation")
{
    for (const float y : { 90.0f, -90.0f })
    {
        const HM::Quaternion q = HM::Quaternion::FromEuler(30.0f, y, 40.0f);
        const HM::Vector3    euler = q.ToEuler();
        CHECK(euler.y() == doctest::Approx(y).epsilon(1e-4));
        CHECK(euler.z() == 0.0f);
        CHECK(SameRotation(HM::Quaternion::FromEuler(euler), q));
    }
}

TEST_CASE("Quaternion - axis-angle turns the basis vectors counterclockwise")
{
    const HM::Quaternion aboutZ = HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 0.0f, 1.0f), 90.0f);
    CHECK(HedgehogTest::NearlyEqual(aboutZ * HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f)));
    CHECK(HedgehogTest::NearlyEqual(aboutZ * HM::Vector3(0.0f, 1.0f, 0.0f), HM::Vector3(-1.0f, 0.0f, 0.0f)));
    CHECK(HedgehogTest::NearlyEqual(aboutZ * HM::Vector3(0.0f, 0.0f, 1.0f), HM::Vector3(0.0f, 0.0f, 1.0f)));

    const HM::Quaternion aboutX = HM::Quaternion::FromAxisAngle(HM::Vector3(2.0f, 0.0f, 0.0f), 90.0f); // not unit
    CHECK(HedgehogTest::NearlyEqual(aboutX.Length(), 1.0f));
    CHECK(HedgehogTest::NearlyEqual(aboutX * HM::Vector3(0.0f, 1.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 1.0f)));

    const HM::Quaternion aboutY = HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 1.0f, 0.0f), 90.0f);
    CHECK(HedgehogTest::NearlyEqual(aboutY * HM::Vector3(0.0f, 0.0f, 1.0f), HM::Vector3(1.0f, 0.0f, 0.0f)));

    // The same turns as the matrices'.
    CHECK(HedgehogTest::NearlyEqual(aboutY.ToMatrix(), HM::Matrix4x4::GetRotationY(HM::ToRadians(90.0f))));
    CHECK(HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 0.0f, 0.0f), 45.0f) == HM::Quaternion());
}

TEST_CASE("Quaternion - Rotate agrees with ToMatrix for random rotations and vectors")
{
    std::mt19937 generator(11u);
    std::uniform_real_distribution<float> coordinate(-10.0f, 10.0f);
    for (int i = 0; i < 100; ++i)
    {
        const HM::Quaternion q = HM::Quaternion::FromEuler(RandomEuler(generator));
        const float x = coordinate(generator);
        const float y = coordinate(generator);
        const float z = coordinate(generator);
        const HM::Vector4 viaMatrix = q.ToMatrix() * HM::Vector4(x, y, z, 1.0f);
        REQUIRE(HedgehogTest::NearlyEqual(q.Rotate(HM::Vector3(x, y, z)),
                                          HM::Vector3(viaMatrix.x(), viaMatrix.y(), viaMatrix.z()), 1e-4f));
    }
}

TEST_CASE("Quaternion - multiplication composes like the matrices: b first, then a")
{
    std::mt19937 generator(3u);
    for (int i = 0; i < 100; ++i)
    {
        const HM::Quaternion a = HM::Quaternion::FromEuler(RandomEuler(generator));
        const HM::Quaternion b = HM::Quaternion::FromEuler(RandomEuler(generator));
        REQUIRE(HedgehogTest::NearlyEqual((a * b).ToMatrix(), a.ToMatrix() * b.ToMatrix()));
    }

    const HM::Quaternion aboutX = HM::Quaternion::FromAxisAngle(HM::Vector3(1.0f, 0.0f, 0.0f), 90.0f);
    const HM::Quaternion aboutZ = HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 0.0f, 1.0f), 90.0f);
    // X turns to Y about Z, then Y turns to Z about X.
    CHECK(HedgehogTest::NearlyEqual((aboutX * aboutZ) * HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 1.0f)));
}

TEST_CASE("Quaternion - inverse times q is the identity")
{
    std::mt19937 generator(5u);
    for (int i = 0; i < 100; ++i)
    {
        const HM::Quaternion q = HM::Quaternion::FromEuler(RandomEuler(generator));
        REQUIRE(NearlyEqual(q.Inverse() * q, HM::Quaternion()));
        REQUIRE(NearlyEqual(q * q.Inverse(), HM::Quaternion()));
        REQUIRE(NearlyEqual(q.Inverse(), q.Conjugate())); // unit length
    }

    // A quaternion that is not unit length inverts too.
    const HM::Quaternion scaled(0.0f, 0.0f, 2.0f, 2.0f);
    CHECK(NearlyEqual(scaled.Inverse() * scaled, HM::Quaternion()));
    CHECK(HM::Quaternion(0.0f, 0.0f, 0.0f, 0.0f).Inverse() == HM::Quaternion());
}

TEST_CASE("Quaternion - Normalize gives unit length, and zero normalizes to the identity")
{
    const HM::Quaternion q = HM::Quaternion(1.0f, 2.0f, 3.0f, 4.0f).Normalize();
    CHECK(HedgehogTest::NearlyEqual(q.Length(), 1.0f));
    CHECK(HedgehogTest::NearlyEqual(q.w() / q.x(), 4.0f));
    CHECK(HM::Quaternion(0.0f, 0.0f, 0.0f, 0.0f).Normalize() == HM::Quaternion());
}

TEST_CASE("Quaternion - Slerp hits its endpoints, halves the angle, and takes the shorter way")
{
    const HM::Vector3    axis(0.0f, 1.0f, 0.0f);
    const HM::Quaternion a = HM::Quaternion::FromAxisAngle(axis, 10.0f);
    const HM::Quaternion b = HM::Quaternion::FromAxisAngle(axis, 130.0f);

    CHECK(NearlyEqual(HM::Quaternion::Slerp(a, b, 0.0f), a));
    CHECK(NearlyEqual(HM::Quaternion::Slerp(a, b, 1.0f), b));
    CHECK(SameRotation(HM::Quaternion::Slerp(a, b, 0.5f), HM::Quaternion::FromAxisAngle(axis, 70.0f)));
    CHECK(SameRotation(HM::Quaternion::Slerp(a, b, 0.25f), HM::Quaternion::FromAxisAngle(axis, 40.0f)));

    // 350 degrees is -10: halfway from 10 goes through 0, not 180.
    const HM::Quaternion wrapped = HM::Quaternion::FromAxisAngle(axis, 350.0f);
    CHECK(SameRotation(HM::Quaternion::Slerp(a, wrapped, 0.5f), HM::Quaternion()));

    // Nearly equal rotations still interpolate to unit length.
    const HM::Quaternion close = HM::Quaternion::FromAxisAngle(axis, 10.01f);
    CHECK(HedgehogTest::NearlyEqual(HM::Quaternion::Slerp(a, close, 0.5f).Length(), 1.0f));
}

TEST_CASE("Quaternion - LookRotation turns -Z to forward and keeps +Y up")
{
    const HM::Vector3 forwardAxis(0.0f, 0.0f, -1.0f);
    const HM::Vector3 upAxis(0.0f, 1.0f, 0.0f);

    CHECK(SameRotation(HM::Quaternion::LookRotation(HM::Vector3(0.0f, 0.0f, -1.0f)), HM::Quaternion()));

    const HM::Quaternion right = HM::Quaternion::LookRotation(HM::Vector3(1.0f, 0.0f, 0.0f));
    CHECK(HedgehogTest::NearlyEqual(right * forwardAxis, HM::Vector3(1.0f, 0.0f, 0.0f)));
    CHECK(HedgehogTest::NearlyEqual(right * upAxis, upAxis));

    const HM::Vector3    diagonal = HM::Vector3(1.0f, 1.0f, -2.0f);
    const HM::Quaternion look     = HM::Quaternion::LookRotation(diagonal * 3.0f);
    CHECK(HedgehogTest::NearlyEqual(look * forwardAxis, diagonal.Normalize()));
    const HM::Vector3 lookUp = look * upAxis;
    CHECK(HedgehogTest::NearlyEqual(HM::Dot(lookUp, diagonal.Normalize()), 0.0f));
    CHECK(lookUp.y() > 0.0f);

    // Looking straight up: another up is chosen, forward still holds.
    const HM::Quaternion skyward = HM::Quaternion::LookRotation(upAxis);
    CHECK(HedgehogTest::NearlyEqual(skyward * forwardAxis, upAxis));
    CHECK(HedgehogTest::NearlyEqual(skyward.Length(), 1.0f));

    CHECK(HM::Quaternion::LookRotation(HM::Vector3(0.0f, 0.0f, 0.0f)) == HM::Quaternion());
}
