#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/EnvironmentComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/EnvironmentSystem.hpp"

#include <memory>
#include <string>

using HedgehogEngine::EnvironmentComponent;
using HX::SceneExtractor;

namespace
{
    // An ECS with the environment component and its entity view.
    struct EnvironmentFixture
    {
        ECS::ECS                                           ecs;
        std::shared_ptr<HedgehogEngine::EnvironmentSystem> environments;

        EnvironmentFixture()
        {
            ecs.Init();
            ecs.RegisterComponent<EnvironmentComponent>();
            environments = ecs.RegisterSystem<HedgehogEngine::EnvironmentSystem>();
            ECS::Signature signature;
            signature.set(ecs.GetComponentType<EnvironmentComponent>());
            ecs.SetSystemSignature<HedgehogEngine::EnvironmentSystem>(signature);
        }

        ECS::Entity Add(const EnvironmentComponent& environment)
        {
            const ECS::Entity entity = ecs.CreateEntity();
            ecs.AddComponent(entity, environment);
            return entity;
        }

        HX::RenderEnvironment Extract(HX::RenderScene& scene) const
        {
            SceneExtractor{}.ExtractEnvironment(ecs, *environments, scene);
            return scene.Environment;
        }
    };

    EnvironmentComponent Environment(const std::string& map, float exposure = 0.0f, bool enabled = true)
    {
        EnvironmentComponent environment;
        environment.Map      = map;
        environment.Exposure = exposure;
        environment.Enabled  = enabled;
        return environment;
    }
}

TEST_CASE("Environment extraction - none gives no environment")
{
    EnvironmentFixture fixture;
    HX::RenderScene    scene;
    CHECK_FALSE(fixture.Extract(scene).Present);
}

TEST_CASE("Environment extraction - one environment's values reach the scene, its map under assets://")
{
    EnvironmentFixture   fixture;
    EnvironmentComponent sky = Environment("Environments\\sky.hdr", 1.5f);
    sky.Intensity            = 0.5f;
    sky.Rotation             = 45.0f;
    sky.ShowSkybox           = false;
    fixture.Add(sky);

    HX::RenderScene             scene;
    const HX::RenderEnvironment environment = fixture.Extract(scene);
    REQUIRE(environment.Present);
    CHECK(environment.MapPath == "assets://Environments/sky.hdr");
    CHECK(environment.Intensity == 0.5f);
    CHECK(environment.RotationDegrees == 45.0f);
    CHECK_FALSE(environment.ShowSkybox);
    CHECK(environment.Exposure == 1.5f);
}

TEST_CASE("Environment extraction - a map naming a mount keeps it, and an empty map stays empty")
{
    EnvironmentFixture fixture;
    fixture.Add(Environment("engine://Content/Sky/day.hdr"));
    HX::RenderScene scene;
    CHECK(fixture.Extract(scene).MapPath == "engine://Content/Sky/day.hdr");

    EnvironmentFixture exposureOnly;
    exposureOnly.Add(Environment("", -1.0f));
    HX::RenderScene             other;
    const HX::RenderEnvironment environment = exposureOnly.Extract(other);
    REQUIRE(environment.Present);
    CHECK(environment.MapPath.empty());
    CHECK(environment.Exposure == -1.0f);
}

TEST_CASE("Environment extraction - the first enabled environment by entity id wins")
{
    EnvironmentFixture fixture;
    const ECS::Entity  disabled = fixture.Add(Environment("Off.hdr", 0.0f, false));
    const ECS::Entity  first    = fixture.Add(Environment("First.hdr"));
    const ECS::Entity  second   = fixture.Add(Environment("Second.hdr"));
    CHECK(disabled < first);
    CHECK(first < second);

    HX::RenderScene scene;
    CHECK(fixture.Extract(scene).MapPath == "assets://First.hdr");

    // With the first gone, the next one by id.
    fixture.ecs.DestroyEntity(first);
    CHECK(fixture.Extract(scene).MapPath == "assets://Second.hdr");

    // None enabled: no environment, whatever the components hold.
    fixture.ecs.GetComponent<EnvironmentComponent>(second).Enabled = false;
    CHECK_FALSE(fixture.Extract(scene).Present);
}

TEST_CASE("Environment extraction - Clear removes the environment and keeps the path's storage")
{
    EnvironmentFixture fixture;
    fixture.Add(Environment("Environments/a_rather_long_environment_map_name.hdr"));
    HX::RenderScene scene;
    REQUIRE(fixture.Extract(scene).Present);
    const size_t capacity = scene.Environment.MapPath.capacity();

    scene.Clear();
    CHECK_FALSE(scene.Environment.Present);
    CHECK(scene.Environment.MapPath.empty());
    CHECK(scene.Environment.MapPath.capacity() == capacity);

    (void)fixture.Extract(scene);
    CHECK(scene.Environment.MapPath.capacity() == capacity);
}
