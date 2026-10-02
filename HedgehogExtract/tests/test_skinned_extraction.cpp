#include "test_helpers.hpp"

#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"

#include "doctest/doctest/doctest.h"

#include <tuple>
#include <vector>

using namespace HXTest;

namespace
{
    // A palette whose matrices are told apart by their translation: (base + joint, 0, 0).
    std::vector<HM::Matrix4x4> MakePalette(size_t jointCount, float base)
    {
        std::vector<HM::Matrix4x4> palette;
        for (size_t joint = 0; joint < jointCount; ++joint)
            palette.push_back(HM::Matrix4x4::GetTranslation(base + static_cast<float>(joint), 0.0f, 0.0f));
        return palette;
    }

    ECS::Entity AddSkinnedEntity(ExtractionFixture& fixture, size_t jointCount, float base)
    {
        const ECS::Entity entity = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());
        HedgehogEngine::AnimatorComponent animator;
        animator.Palette = MakePalette(jointCount, base);
        fixture.ecs.AddComponent(entity, animator);
        return entity;
    }

    void Extract(ExtractionFixture& fixture, HX::RenderScene& scene)
    {
        HX::SceneExtractor extractor;
        extractor.Extract(fixture.ecs, *fixture.renderSystem, *fixture.lightSystem, *fixture.cameraSystem, scene);
    }

    const HX::RenderInstance& FindInstance(const HX::RenderScene& scene, ECS::Entity entity)
    {
        for (const HX::RenderInstance& instance : scene.Instances)
            if (instance.SourceId == static_cast<uint64_t>(entity))
                return instance;
        FAIL("no instance for the entity");
        return scene.Instances.front();
    }
}

TEST_CASE("Skinned extraction appends each animated instance's palette contiguously with its offset")
{
    ExtractionFixture fixture;
    const ECS::Entity first  = AddSkinnedEntity(fixture, 2, 10.0f);
    const ECS::Entity second = AddSkinnedEntity(fixture, 3, 20.0f);
    const ECS::Entity third  = AddSkinnedEntity(fixture, 1, 30.0f);

    HX::RenderScene scene;
    Extract(fixture, scene);

    REQUIRE(scene.JointMatrices.size() == 6u);
    uint32_t expectedOffset = 0;
    for (const auto& [entity, jointCount, base] :
         { std::tuple{ first, 2u, 10.0f }, std::tuple{ second, 3u, 20.0f }, std::tuple{ third, 1u, 30.0f } })
    {
        const HX::RenderInstance& instance = FindInstance(scene, entity);
        CHECK(instance.PaletteOffset == expectedOffset);
        CHECK(instance.JointCount == jointCount);
        for (uint32_t joint = 0; joint < jointCount; ++joint)
            CHECK(scene.JointMatrices[instance.PaletteOffset + joint][3][0] == base + static_cast<float>(joint));
        expectedOffset += jointCount;
    }
}

TEST_CASE("Static instances, and animators without a palette, have no joints")
{
    ExtractionFixture fixture;
    const ECS::Entity plain = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());
    const ECS::Entity empty = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());
    fixture.ecs.AddComponent(empty, HedgehogEngine::AnimatorComponent{});

    HX::RenderScene scene;
    Extract(fixture, scene);

    CHECK(FindInstance(scene, plain).JointCount == 0u);
    CHECK(FindInstance(scene, empty).JointCount == 0u);
    CHECK(scene.JointMatrices.empty());
}

TEST_CASE("A skinned instance's bounds are its bind-pose bounds inflated by the margin")
{
    ExtractionFixture fixture;
    const ECS::Entity skinned = AddSkinnedEntity(fixture, 2, 0.0f);
    const ECS::Entity plain   = fixture.AddRenderableEntity(1, 1, 0, HM::Matrix4x4::GetIdentity());

    HX::RenderScene scene;
    Extract(fixture, scene);

    // The unit cube (half extent 0.5), grown on every side by half its largest extent: 0.5.
    CHECK(FindInstance(scene, skinned).WorldBounds.GetHalfExtents() == HM::Vector3(1.0f, 1.0f, 1.0f));
    CHECK(FindInstance(scene, plain).WorldBounds.GetHalfExtents() == HM::Vector3(0.5f, 0.5f, 0.5f));
}

TEST_CASE("Re-extracting skinned instances into a cleared RenderScene reuses the palette storage")
{
    ExtractionFixture fixture;
    AddSkinnedEntity(fixture, 4, 0.0f);
    AddSkinnedEntity(fixture, 3, 10.0f);

    HX::RenderScene scene;
    Extract(fixture, scene);
    REQUIRE(scene.JointMatrices.size() == 7u);
    const HM::Matrix4x4* storage  = scene.JointMatrices.data();
    const size_t         capacity = scene.JointMatrices.capacity();

    for (int frame = 0; frame < 5; ++frame)
    {
        scene.Clear();
        Extract(fixture, scene);
        CHECK(scene.JointMatrices.size() == 7u);
        CHECK(scene.JointMatrices.capacity() == capacity);
        CHECK(scene.JointMatrices.data() == storage);
    }
}
