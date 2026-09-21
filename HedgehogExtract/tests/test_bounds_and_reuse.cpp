#include "test_helpers.hpp"

#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"

#include "doctest/doctest/doctest.h"

using namespace HXTest;

TEST_CASE("SceneExtractor produces world bounds that follow the instance's world matrix")
{
    ExtractionFixture fixture;
    const HM::Vector3 translation(10.0f, -4.0f, 2.0f);
    const HM::Matrix4x4 worldMatrix = HM::Matrix4x4::GetTranslation(translation.x(), translation.y(), translation.z());
    fixture.AddRenderableEntity(1, 1, 0, worldMatrix);

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Instances.size() == 1);
    const HM::AABB& bounds = scene.Instances[0].WorldBounds;

    // Placeholder local bounds are a unit cube centered on the origin (see kUnitLocalBounds in
    // SceneExtractor.cpp) — translating the instance must translate the world bounds the same way.
    CHECK(bounds.GetCenter() == translation);
    CHECK(bounds.GetHalfExtents() == HM::Vector3(0.5f, 0.5f, 0.5f));
}

TEST_CASE("SceneExtractor's world bounds grow with a scaled world matrix")
{
    ExtractionFixture fixture;
    const HM::Matrix4x4 worldMatrix = HM::Matrix4x4::GetScale(2.0f, 4.0f, 1.0f);
    fixture.AddRenderableEntity(1, 1, 0, worldMatrix);

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Instances.size() == 1);
    const HM::AABB& bounds = scene.Instances[0].WorldBounds;
    CHECK(bounds.GetHalfExtents() == HM::Vector3(1.0f, 2.0f, 0.5f));
}

TEST_CASE("RenderScene reuses its capacity across Clear() + re-extraction")
{
    ExtractionFixture fixture;
    fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());
    fixture.AddLightEntity(HedgehogEngine::LightType::PointLight, HM::Vector3(0.0f, 0.0f, 0.0f),
                            HM::Vector3(0.0f, -1.0f, 0.0f));
    fixture.AddCameraEntity(HM::Matrix4x4::GetIdentity());

    HX::RenderScene scene;
    HX::SceneExtractor extractor;

    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);
    REQUIRE(scene.Instances.size() == 1);
    REQUIRE(scene.Lights.size() == 1);
    REQUIRE(scene.Cameras.size() == 1);

    const size_t instanceCapacity = scene.Instances.capacity();
    const size_t lightCapacity    = scene.Lights.capacity();
    const size_t cameraCapacity   = scene.Cameras.capacity();

    // Steady state: clear (not reassign/shrink) and re-extract the same scene several times.
    // Capacity must not grow — a growth would mean a reallocation, i.e. a per-frame allocation.
    for (int i = 0; i < 5; ++i)
    {
        scene.Clear();
        extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

        CHECK(scene.Instances.size() == 1);
        CHECK(scene.Lights.size() == 1);
        CHECK(scene.Cameras.size() == 1);
        CHECK(scene.Instances.capacity() == instanceCapacity);
        CHECK(scene.Lights.capacity() == lightCapacity);
        CHECK(scene.Cameras.capacity() == cameraCapacity);
    }
}
