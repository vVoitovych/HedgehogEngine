#include "doctest/doctest/doctest.h"

#include "test_common.hpp"

#include "HedgehogMath/api/AABB.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

using HedgehogTest::NearlyEqual;

TEST_CASE("AABB::IntersectRay - hit from outside")
{
    const HM::AABB box(HM::Vector3(-1.0f, -1.0f, -1.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    float t = 0.0f;
    // Ray from x=-5 toward +X enters the box at x=-1 → t = 4.
    CHECK(box.IntersectRay(HM::Vector3(-5.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), t));
    CHECK(NearlyEqual(t, 4.0f));
}

TEST_CASE("AABB::IntersectRay - miss (passes above)")
{
    const HM::AABB box(HM::Vector3(-1.0f, -1.0f, -1.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    float t = 0.0f;
    CHECK_FALSE(box.IntersectRay(HM::Vector3(-5.0f, 5.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), t));
}

TEST_CASE("AABB::IntersectRay - box entirely behind the origin")
{
    const HM::AABB box(HM::Vector3(-1.0f, -1.0f, -1.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    float t = 0.0f;
    CHECK_FALSE(box.IntersectRay(HM::Vector3(5.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), t));
}

TEST_CASE("AABB::IntersectRay - origin inside the box")
{
    const HM::AABB box(HM::Vector3(-1.0f, -1.0f, -1.0f), HM::Vector3(1.0f, 1.0f, 1.0f));
    float t = 0.0f;
    CHECK(box.IntersectRay(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), t));
}

TEST_CASE("UnprojectNdc - project / unproject round-trips")
{
    // Same view-projection shape the engine uses, including the Vulkan Y flip.
    HM::Matrix4x4 view = HM::Matrix4x4::LookAt(
        HM::Vector3(0.0f, 0.0f, 5.0f), HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f));
    HM::Matrix4x4 proj = HM::Matrix4x4::Perspective(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    proj[1][1] *= -1.0f;

    const HM::Matrix4x4 vp = proj * view;
    bool ok = false;
    const HM::Matrix4x4 invVp = vp.Inverse(ok);
    REQUIRE(ok);

    const HM::Vector3 world(0.7f, -0.4f, 1.0f);
    // Project (row-vector convention): clip = [world, 1] * vp; ndc = clip.xyz / clip.w.
    const HM::Vector4 clip = HM::Vector4(world.x(), world.y(), world.z(), 1.0f) * vp;
    const HM::Vector3 ndc(clip.x() / clip.w(), clip.y() / clip.w(), clip.z() / clip.w());

    const HM::Vector3 recovered = HM::UnprojectNdc(invVp, ndc);
    CHECK(NearlyEqual(recovered, world, 1e-3f));
}
