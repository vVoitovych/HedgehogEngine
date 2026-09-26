#include "HedgehogExtract/api/CameraMath.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/ScenePicker.hpp"

#include "doctest/doctest/doctest.h"

namespace
{
    HX::RenderInstance Box(uint64_t sourceId, const HM::Vector3& min, const HM::Vector3& max, uint32_t layer = 0)
    {
        HX::RenderInstance instance;
        instance.WorldBounds = HM::AABB(min, max);
        instance.SourceId    = sourceId;
        instance.Layer       = layer;
        return instance;
    }

    HX::Ray RayAlongX(float y)
    {
        HX::Ray ray;
        ray.Origin    = HM::Vector3(-10.0f, y, 0.0f);
        ray.Direction = HM::Vector3(1.0f, 0.0f, 0.0f);
        return ray;
    }

    // A camera at (0, -10, 0) looking along +Y: its world matrix's columns are the camera's right,
    // up and back axes, then its position.
    HX::RenderCamera CameraLookingAlongY(HX::CameraProjectionType projection)
    {
        HX::RenderCamera camera;
        camera.WorldMatrix = HM::Matrix4x4::GetIdentity();
        camera.WorldMatrix[0] = HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
        camera.WorldMatrix[1] = HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f);
        camera.WorldMatrix[2] = HM::Vector4(0.0f, -1.0f, 0.0f, 0.0f);
        camera.WorldMatrix[3] = HM::Vector4(0.0f, -10.0f, 0.0f, 1.0f);
        camera.ProjectionType = projection;
        camera.Fov            = 60.0f;
        camera.OrthoSize      = 10.0f;
        return camera;
    }
}

TEST_CASE("IntersectRayAABB returns the entry distance, and misses boxes beside or behind the ray")
{
    const HM::AABB box(HM::Vector3(-1.0f, -1.0f, -1.0f), HM::Vector3(1.0f, 1.0f, 1.0f));

    const std::optional<float> hit = HX::IntersectRayAABB(RayAlongX(0.0f), box);
    REQUIRE(hit.has_value());
    CHECK(*hit == doctest::Approx(9.0f));

    CHECK_FALSE(HX::IntersectRayAABB(RayAlongX(2.0f), box).has_value()); // beside

    HX::Ray away = RayAlongX(0.0f);
    away.Direction = HM::Vector3(-1.0f, 0.0f, 0.0f);
    CHECK_FALSE(HX::IntersectRayAABB(away, box).has_value()); // behind

    HX::Ray inside;
    inside.Direction = HM::Vector3(0.0f, 0.0f, 1.0f);
    const std::optional<float> fromInside = HX::IntersectRayAABB(inside, box);
    REQUIRE(fromInside.has_value());
    CHECK(*fromInside == doctest::Approx(0.0f));
}

TEST_CASE("PickInstance returns the nearest hit's entity, and nothing for empty space")
{
    HX::RenderScene scene;
    scene.Instances.push_back(Box(7, HM::Vector3(4.0f, -1.0f, -1.0f), HM::Vector3(6.0f, 1.0f, 1.0f)));
    scene.Instances.push_back(Box(3, HM::Vector3(0.0f, -1.0f, -1.0f), HM::Vector3(2.0f, 1.0f, 1.0f)));

    CHECK(HX::PickInstance(scene, RayAlongX(0.0f)) == std::optional<uint64_t>(3));
    CHECK_FALSE(HX::PickInstance(scene, RayAlongX(5.0f)).has_value());
}

TEST_CASE("PickInstance skips layers outside the mask, and the editor layer by default")
{
    HX::RenderScene scene;
    scene.Instances.push_back(Box(1, HM::Vector3(0.0f, -1.0f, -1.0f), HM::Vector3(2.0f, 1.0f, 1.0f), HX::EDITOR_LAYER));
    scene.Instances.push_back(Box(2, HM::Vector3(4.0f, -1.0f, -1.0f), HM::Vector3(6.0f, 1.0f, 1.0f), 3));

    CHECK(HX::PickInstance(scene, RayAlongX(0.0f)) == std::optional<uint64_t>(2));
    CHECK_FALSE(HX::PickInstance(scene, RayAlongX(0.0f), ~(HX::EDITOR_LAYER_MASK | (1u << 3))).has_value());
    CHECK(HX::PickInstance(scene, RayAlongX(0.0f), 0xFFFFFFFFu) == std::optional<uint64_t>(1));
}

TEST_CASE("MakePickRay goes through the image point it is given, for both projections")
{
    for (const HX::CameraProjectionType projection :
         { HX::CameraProjectionType::Perspective, HX::CameraProjectionType::Orthographic })
    {
        CAPTURE(static_cast<int>(projection));
        const HX::RenderCamera camera = CameraLookingAlongY(projection);

        // The centre of the image looks straight ahead.
        const HX::Ray centre = HX::MakePickRay(camera, 16.0f / 9.0f, 0.5f, 0.5f);
        CHECK(centre.Direction.x() == doctest::Approx(0.0f).epsilon(1e-4));
        CHECK(centre.Direction.y() == doctest::Approx(1.0f).epsilon(1e-4));
        CHECK(centre.Direction.z() == doctest::Approx(0.0f).epsilon(1e-4));
        CHECK(centre.Origin.y() <= -9.8f); // at the eye, or on the near plane

        // The top of the image is above the centre, the right of it to the right.
        HX::RenderScene scene;
        scene.Instances.push_back(Box(1, HM::Vector3(-0.5f, -0.5f, 1.5f), HM::Vector3(0.5f, 0.5f, 2.5f)));  // above
        scene.Instances.push_back(Box(2, HM::Vector3(1.5f, -0.5f, -0.5f), HM::Vector3(2.5f, 0.5f, 0.5f)));  // right
        scene.Instances.push_back(Box(3, HM::Vector3(-0.5f, -0.5f, -0.5f), HM::Vector3(0.5f, 0.5f, 0.5f))); // centre

        // Where a point 2 units off-axis, 10 ahead, lands in a square image: tan(atan(0.2)) / tan(30
        // degrees) of the half-image in perspective, 2 of the 5-unit half-height orthographically.
        const float   offset = projection == HX::CameraProjectionType::Orthographic ? 0.2f : 0.1732f;
        const HX::Ray up     = HX::MakePickRay(camera, 1.0f, 0.5f, 0.5f - offset);
        const HX::Ray right  = HX::MakePickRay(camera, 1.0f, 0.5f + offset, 0.5f);
        CHECK(HX::PickInstance(scene, centre) == std::optional<uint64_t>(3));
        CHECK(HX::PickInstance(scene, up) == std::optional<uint64_t>(1));
        CHECK(HX::PickInstance(scene, right) == std::optional<uint64_t>(2));
    }
}
