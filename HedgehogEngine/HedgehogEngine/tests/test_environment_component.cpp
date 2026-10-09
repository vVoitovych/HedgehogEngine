#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/EnvironmentComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/EnvironmentSystem.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <algorithm>
#include <string>

using namespace HedgehogEngine;

TEST_CASE("Environment component - its defaults")
{
    const EnvironmentComponent environment;
    CHECK(environment.Enabled);
    CHECK(environment.Map.empty());
    CHECK(environment.Intensity == 1.0f);
    CHECK(environment.Rotation == 0.0f);
    CHECK(environment.ShowSkybox);
    CHECK(environment.Exposure == 0.0f);
}

TEST_CASE("Environment component - a scene holding one round-trips byte for byte, and the view tracks it")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    SceneManager&     scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    EnvironmentComponent environment;
    environment.Enabled    = false;
    environment.Map        = "Environments/sky.hdr";
    environment.Intensity  = 0.75f;
    environment.Rotation   = 90.0f;
    environment.ShowSkybox = false;
    environment.Exposure   = -1.5f;
    ecs.AddComponent(entity, environment);

    const auto& view = context.GetEnvironmentSystem()->GetEntities();
    CHECK(std::find(view.begin(), view.end(), entity) != view.end());

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    for (const char* key : { "EnvironmentComponent", "Map: Environments/sky.hdr", "Intensity", "Rotation", "ShowSkybox",
                             "Exposure" })
    {
        CAPTURE(key);
        CHECK(snapshot.Yaml.find(key) != std::string::npos);
    }

    REQUIRE(scenes.RestoreSnapshot(snapshot));
    REQUIRE(ecs.HasComponent<EnvironmentComponent>(entity));
    const EnvironmentComponent& restored = ecs.GetComponent<EnvironmentComponent>(entity);
    CHECK_FALSE(restored.Enabled);
    CHECK(restored.Map == "Environments/sky.hdr");
    CHECK(restored.Intensity == 0.75f);
    CHECK(restored.Rotation == 90.0f);
    CHECK_FALSE(restored.ShowSkybox);
    CHECK(restored.Exposure == -1.5f);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}
