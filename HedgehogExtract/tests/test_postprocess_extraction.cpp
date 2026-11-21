#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/PostProcessComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PostProcessSystem.hpp"

#include <memory>
#include <string>

using HedgehogEngine::PostProcessComponent;
using HX::SceneExtractor;

namespace
{
    struct PostProcessFixture
    {
        ECS::ECS                                           ecs;
        std::shared_ptr<HedgehogEngine::PostProcessSystem> view;

        PostProcessFixture()
        {
            ecs.Init();
            ecs.RegisterComponent<PostProcessComponent>();
            view = ecs.RegisterSystem<HedgehogEngine::PostProcessSystem>();
            ECS::Signature signature;
            signature.set(ecs.GetComponentType<PostProcessComponent>());
            ecs.SetSystemSignature<HedgehogEngine::PostProcessSystem>(signature);
        }

        ECS::Entity Add(const PostProcessComponent& post)
        {
            const ECS::Entity entity = ecs.CreateEntity();
            ecs.AddComponent(entity, post);
            return entity;
        }

        const HX::RenderPostProcess& Extract(HX::RenderScene& scene) const
        {
            SceneExtractor{}.ExtractPostProcess(ecs, *view, scene);
            return scene.PostProcess;
        }
    };

    PostProcessComponent Post(const std::string& lut, float bloom = 0.05f, bool enabled = true)
    {
        PostProcessComponent post;
        post.ColorLut       = lut;
        post.BloomIntensity = bloom;
        post.Enabled        = enabled;
        return post;
    }
}

TEST_CASE("Post-process extraction - none gives nothing")
{
    PostProcessFixture fixture;
    HX::RenderScene    scene;
    CHECK_FALSE(fixture.Extract(scene).Present);
}

TEST_CASE("Post-process extraction - one component's values reach the scene, its LUT under assets://")
{
    PostProcessFixture   fixture;
    PostProcessComponent post = Post("Textures\\Lut.png", 0.2f);
    post.AntiAliasing         = HedgehogEngine::PostProcessAntiAliasing::None;
    post.SsaoEnabled          = true;
    post.SsaoRadius           = 0.8f;
    post.SsaoIntensity        = 2.0f;
    post.SsaoBias             = 0.05f;
    post.BloomEnabled         = true;
    post.BloomThreshold       = 1.5f;
    post.BloomKnee            = 0.25f;
    post.ColorLutContribution = 0.75f;
    fixture.Add(post);

    HX::RenderScene              scene;
    const HX::RenderPostProcess& out = fixture.Extract(scene);
    REQUIRE(out.Present);
    CHECK_FALSE(out.Fxaa);
    CHECK(out.SsaoEnabled);
    CHECK(out.SsaoRadius == 0.8f);
    CHECK(out.SsaoIntensity == 2.0f);
    CHECK(out.SsaoBias == 0.05f);
    CHECK(out.BloomEnabled);
    CHECK(out.BloomIntensity == 0.2f);
    CHECK(out.BloomThreshold == 1.5f);
    CHECK(out.BloomKnee == 0.25f);
    CHECK(out.ColorLutPath == "assets://Textures/Lut.png");
    CHECK(out.ColorLutContribution == 0.75f);

    // A LUT naming a mount is kept, and none stays empty.
    fixture.ecs.GetComponent<PostProcessComponent>(0).ColorLut = "engine://Content/Lut.png";
    CHECK(fixture.Extract(scene).ColorLutPath == "engine://Content/Lut.png");
    fixture.ecs.GetComponent<PostProcessComponent>(0).ColorLut.clear();
    CHECK(fixture.Extract(scene).ColorLutPath.empty());
}

TEST_CASE("Post-process extraction - the first enabled one by entity id wins, and Clear keeps the path's storage")
{
    PostProcessFixture fixture;
    fixture.Add(Post("off.png", 9.0f, false));
    const ECS::Entity first = fixture.Add(Post("first.png", 0.1f));
    fixture.Add(Post("second.png", 0.3f));

    HX::RenderScene scene;
    CHECK(fixture.Extract(scene).BloomIntensity == 0.1f);
    fixture.ecs.DestroyEntity(first);
    CHECK(fixture.Extract(scene).BloomIntensity == 0.3f);

    const size_t capacity = scene.PostProcess.ColorLutPath.capacity();
    scene.Clear();
    CHECK_FALSE(scene.PostProcess.Present);
    CHECK(scene.PostProcess.ColorLutPath.empty());
    CHECK(scene.PostProcess.ColorLutPath.capacity() == capacity);
}
