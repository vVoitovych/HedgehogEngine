#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/PostProcessComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PostProcessSystem.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <algorithm>
#include <string>

using namespace HedgehogEngine;

TEST_CASE("Post-process component - its defaults")
{
    const PostProcessComponent post;
    CHECK(post.Enabled);
    CHECK(post.AntiAliasing == PostProcessAntiAliasing::Fxaa);
    CHECK_FALSE(post.SsaoEnabled);
    CHECK(post.SsaoRadius == 0.5f);
    CHECK(post.SsaoIntensity == 1.0f);
    CHECK(post.SsaoBias == 0.025f);
    CHECK_FALSE(post.BloomEnabled);
    CHECK(post.BloomIntensity == 0.05f);
    CHECK(post.BloomThreshold == 1.0f);
    CHECK(post.BloomKnee == 0.5f);
    CHECK(post.ColorLut.empty());
    CHECK(post.ColorLutContribution == 1.0f);
}

TEST_CASE("Post-process component - a scene holding one round-trips byte for byte, and the view tracks it")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    SceneManager&     scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    PostProcessComponent post;
    post.AntiAliasing         = PostProcessAntiAliasing::None;
    post.SsaoEnabled          = true;
    post.SsaoRadius           = 0.75f;
    post.BloomEnabled         = true;
    post.BloomThreshold       = 2.0f;
    post.ColorLut             = "Textures/Lut.png";
    post.ColorLutContribution = 0.5f;
    ecs.AddComponent(entity, post);

    const auto& view = context.GetPostProcessSystem()->GetEntities();
    CHECK(std::find(view.begin(), view.end(), entity) != view.end());

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    for (const char* key : { "PostProcessComponent", "AntiAliasing", "SsaoRadius", "BloomThreshold",
                             "ColorLut: Textures/Lut.png", "ColorLutContribution" })
    {
        CAPTURE(key);
        CHECK(snapshot.Yaml.find(key) != std::string::npos);
    }

    REQUIRE(scenes.RestoreSnapshot(snapshot));
    REQUIRE(ecs.HasComponent<PostProcessComponent>(entity));
    const PostProcessComponent& restored = ecs.GetComponent<PostProcessComponent>(entity);
    CHECK(restored.AntiAliasing == PostProcessAntiAliasing::None);
    CHECK(restored.SsaoEnabled);
    CHECK(restored.SsaoRadius == 0.75f);
    CHECK(restored.BloomEnabled);
    CHECK(restored.BloomThreshold == 2.0f);
    CHECK(restored.ColorLut == "Textures/Lut.png");
    CHECK(restored.ColorLutContribution == 0.5f);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}
