#include "test_helpers.hpp"

#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"

#include "doctest/doctest/doctest.h"

using namespace HXTest;

TEST_CASE("SceneExtractor extracts a visible instance with resolved indices")
{
    ExtractionFixture fixture;
    const HM::Matrix4x4 worldMatrix = HM::Matrix4x4::GetTranslation(1.0f, 2.0f, 3.0f);
    const ECS::Entity entity = fixture.AddRenderableEntity(/*meshIndex*/ 7, /*materialIndex*/ 3,
                                                             /*layer*/ 5, worldMatrix);

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Instances.size() == 1);
    const HX::RenderInstance& instance = scene.Instances[0];
    CHECK(instance.MeshIndex == 7);
    CHECK(instance.MaterialIndex == 3);
    CHECK(instance.Layer == 5);
    CHECK(instance.WorldMatrix == worldMatrix);
}

TEST_CASE("SceneExtractor skips instances that are invisible or missing resolved indices")
{
    ExtractionFixture fixture;

    // Invisible.
    const ECS::Entity invisible = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());
    fixture.ecs.GetComponent<HedgehogEngine::RenderComponent>(invisible).IsVisible = false;

    // Visible but no MaterialIndex resolved yet.
    const ECS::Entity unresolvedMaterial = fixture.ecs.CreateEntity();
    HedgehogEngine::RenderComponent renderComponent;
    renderComponent.IsVisible = true;
    fixture.ecs.AddComponent(unresolvedMaterial, renderComponent);
    fixture.ecs.AddComponent(unresolvedMaterial, HedgehogEngine::MeshComponent{});
    fixture.ecs.AddComponent(unresolvedMaterial, HedgehogEngine::TransformComponent{});

    // Has RenderComponent but no MeshComponent/TransformComponent at all.
    const ECS::Entity noMesh = fixture.ecs.CreateEntity();
    HedgehogEngine::RenderComponent renderOnly;
    renderOnly.IsVisible     = true;
    renderOnly.MaterialIndex = 1;
    fixture.ecs.AddComponent(noMesh, renderOnly);

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    CHECK(scene.Instances.empty());
}

TEST_CASE("SceneExtractor round-trips SourceId back to the originating entity")
{
    ExtractionFixture fixture;
    const ECS::Entity entity = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Instances.size() == 1);
    CHECK(scene.Instances[0].SourceId == static_cast<uint64_t>(entity));
}

TEST_CASE("SceneExtractor extracts an enabled light with its resolved fields")
{
    ExtractionFixture fixture;
    const HM::Vector3 position(1.0f, 2.0f, 3.0f);
    const HM::Vector3 direction(0.0f, -1.0f, 0.0f);
    const ECS::Entity entity = fixture.AddLightEntity(HedgehogEngine::LightType::PointLight, position, direction);

    auto& light = fixture.ecs.GetComponent<HedgehogEngine::LightComponent>(entity);
    light.Intensity   = 2.5f;
    light.CastShadows = true;

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Lights.size() == 1);
    const HX::RenderLight& extracted = scene.Lights[0];
    CHECK(extracted.Type == HX::LightType::Point);
    CHECK(extracted.Position == position);
    CHECK(extracted.Direction == direction);
    CHECK(extracted.Intensity == doctest::Approx(2.5f));
    CHECK(extracted.CastShadows == true);
    CHECK(extracted.SourceId == static_cast<uint64_t>(entity));
}

TEST_CASE("SceneExtractor skips disabled lights")
{
    ExtractionFixture fixture;
    const ECS::Entity entity = fixture.AddLightEntity(HedgehogEngine::LightType::DirectionLight,
                                                        HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, -1.0f, 0.0f));
    fixture.ecs.GetComponent<HedgehogEngine::LightComponent>(entity).Enable = false;

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    CHECK(scene.Lights.empty());
}

TEST_CASE("SceneExtractor extracts an enabled camera with its resolved fields")
{
    ExtractionFixture fixture;
    const HM::Matrix4x4 worldMatrix = HM::Matrix4x4::GetTranslation(0.0f, 5.0f, -10.0f);
    const ECS::Entity entity = fixture.AddCameraEntity(worldMatrix);

    auto& camera = fixture.ecs.GetComponent<HedgehogEngine::CameraComponent>(entity);
    camera.Fov         = 75.0f;
    camera.LayerMask   = 0x1u;
    camera.GraphName   = "editor-scene";
    camera.Priority    = 3;

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    REQUIRE(scene.Cameras.size() == 1);
    const HX::RenderCamera& extracted = scene.Cameras[0];
    CHECK(extracted.WorldMatrix == worldMatrix);
    CHECK(extracted.Fov == doctest::Approx(75.0f));
    CHECK(extracted.LayerMask == 0x1u);
    CHECK(extracted.GraphName == "editor-scene");
    CHECK(extracted.Priority == 3);
    CHECK(extracted.ProjectionType == HX::CameraProjectionType::Perspective);
    CHECK(extracted.SourceId == static_cast<uint64_t>(entity));
}

TEST_CASE("SceneExtractor skips a disabled camera and one missing TransformComponent")
{
    ExtractionFixture fixture;

    const ECS::Entity disabled = fixture.AddCameraEntity(HM::Matrix4x4::GetIdentity());
    fixture.ecs.GetComponent<HedgehogEngine::CameraComponent>(disabled).IsEnabled = false;

    const ECS::Entity noTransform = fixture.ecs.CreateEntity();
    fixture.ecs.AddComponent(noTransform, HedgehogEngine::CameraComponent{});

    HX::RenderScene scene;
    HX::SceneExtractor extractor;
    extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);

    CHECK(scene.Cameras.empty());
}
