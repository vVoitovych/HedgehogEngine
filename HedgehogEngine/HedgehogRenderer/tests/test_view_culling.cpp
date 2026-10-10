#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogExtract/api/CameraMath.hpp"

#include "doctest/doctest/doctest.h"

#include <vector>

using namespace Renderer;

namespace
{
    // A camera at the origin looking down -Z (an identity world matrix), with a 90 degree view.
    HM::Matrix4x4 ViewProj()
    {
        HX::RenderCamera camera;
        camera.WorldMatrix = HM::Matrix4x4::GetIdentity();
        camera.Fov         = 90.0f;
        camera.NearPlane   = 0.1f;
        camera.FarPlane    = 100.0f;
        return HX::MakeProjection(camera, 1.0f) * HX::MakeViewMatrix(camera);
    }

    HX::RenderInstance At(uint64_t sourceId, const HM::Vector3& centre, uint32_t layer = 0)
    {
        const HM::Vector3 half(0.5f, 0.5f, 0.5f);
        HX::RenderInstance instance;
        instance.WorldBounds = HM::AABB(centre - half, centre + half);
        instance.SourceId    = sourceId;
        instance.Layer       = layer;
        return instance;
    }

    std::vector<uint64_t> Ids(const std::vector<HX::RenderInstance>& instances)
    {
        std::vector<uint64_t> ids;
        for (const HX::RenderInstance& instance : instances)
            ids.push_back(instance.SourceId);
        return ids;
    }
}

TEST_CASE("A view keeps the instances its frustum sees, and none behind or beside it")
{
    const HX::RenderInstance instances[] = {
        At(1, HM::Vector3(0.0f, 0.0f, -10.0f)),  // ahead
        At(2, HM::Vector3(0.0f, 0.0f, 10.0f)),   // behind
        At(3, HM::Vector3(50.0f, 0.0f, -10.0f)), // far to the right
        At(4, HM::Vector3(0.0f, 0.0f, -200.0f)), // past the far plane
        At(5, HM::Vector3(10.2f, 0.0f, -10.0f)), // straddling the right edge
    };

    ViewInstances culled;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), culled);
    CHECK(Ids(culled.Opaque) == std::vector<uint64_t>{ 1, 5 });
    CHECK(culled.Overlay.empty());
}

TEST_CASE("An instance on a layer outside a view's mask is absent from that view and present in others")
{
    const HX::RenderInstance instances[] = { At(1, HM::Vector3(0.0f, 0.0f, -10.0f), 0),
                                             At(2, HM::Vector3(1.0f, 0.0f, -10.0f), 4) };

    ViewInstances withoutLayer4;
    CullViewInstances(instances, ~(1u << 4), ViewProj(), withoutLayer4);
    CHECK(Ids(withoutLayer4.Opaque) == std::vector<uint64_t>{ 1 });

    ViewInstances everything;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), everything);
    CHECK(Ids(everything.Opaque) == std::vector<uint64_t>{ 1, 2 });
}

TEST_CASE("Editor-layer instances are a view's overlay, only when its mask includes the layer")
{
    const HX::RenderInstance instances[] = { At(1, HM::Vector3(0.0f, 0.0f, -10.0f)),
                                             At(1, HM::Vector3(0.0f, 0.0f, -10.0f), HX::EDITOR_LAYER) };

    ViewInstances editor;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), editor);
    CHECK(Ids(editor.Opaque) == std::vector<uint64_t>{ 1 });
    CHECK(Ids(editor.Overlay) == std::vector<uint64_t>{ 1 });

    ViewInstances game;
    CullViewInstances(instances, ~HX::EDITOR_LAYER_MASK, ViewProj(), game);
    CHECK(Ids(game.Opaque) == std::vector<uint64_t>{ 1 });
    CHECK(game.Overlay.empty());

    std::vector<HX::RenderInstance> scene;
    CollectSceneInstances(instances, scene);
    REQUIRE(scene.size() == 1);
    CHECK(scene[0].Layer == 0);
}

TEST_CASE("Re-culling the same scene reuses the view's capacity")
{
    const HX::RenderInstance instances[] = { At(1, HM::Vector3(0.0f, 0.0f, -10.0f)),
                                             At(2, HM::Vector3(0.0f, 0.0f, -10.0f), HX::EDITOR_LAYER) };
    ViewInstances culled;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), culled);
    const size_t opaque  = culled.Opaque.capacity();
    const size_t overlay = culled.Overlay.capacity();
    for (int i = 0; i < 3; ++i)
    {
        CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), culled);
        CHECK(culled.Opaque.capacity() == opaque);
        CHECK(culled.Overlay.capacity() == overlay);
    }
}

TEST_CASE("Skinned instances are culled like the rest and kept apart from the rigid ones")
{
    const auto skinned = [](uint64_t sourceId, const HM::Vector3& centre)
    {
        HX::RenderInstance instance = At(sourceId, centre);
        instance.JointCount         = 2;
        return instance;
    };
    const HX::RenderInstance instances[] = { At(1, HM::Vector3(0.0f, 0.0f, -10.0f)),
                                             skinned(2, HM::Vector3(1.0f, 0.0f, -10.0f)),
                                             skinned(3, HM::Vector3(0.0f, 0.0f, 10.0f)) }; // behind

    ViewInstances view;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), view);
    CHECK(Ids(view.Opaque) == std::vector<uint64_t>{ 1 });
    CHECK(Ids(view.Skinned) == std::vector<uint64_t>{ 2 });
    CHECK(view.Overlay.empty());

    // The shadow casters keep every scene instance, skinned ones too.
    std::vector<HX::RenderInstance> scene;
    CollectSceneInstances(instances, scene);
    CHECK(Ids(scene) == std::vector<uint64_t>{ 1, 2, 3 });
}

TEST_CASE("Instances are split by their material's alpha mode, rigid and skinned apart")
{
    using HedgehogEngine::MaterialAlphaMode;
    const MaterialDrawInfo materials[] = { { MaterialAlphaMode::Opaque, false },
                                           { MaterialAlphaMode::Cutoff, false },
                                           { MaterialAlphaMode::Transparent, true } };
    std::vector<HX::RenderInstance> instances;
    for (uint64_t id = 0; id < 8; ++id)
    {
        HX::RenderInstance instance = At(id, HM::Vector3(0.0f, 0.0f, -5.0f));
        instance.MaterialIndex      = id % 4; // material 3 has no draw info: opaque
        instance.JointCount         = id >= 4 ? 2 : 0;
        instances.push_back(instance);
    }

    ViewInstances view;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), view, materials);
    CHECK(Ids(view.Opaque) == std::vector<uint64_t>{ 0, 3 });
    CHECK(Ids(view.Cutoff) == std::vector<uint64_t>{ 1 });
    CHECK(Ids(view.Transparent) == std::vector<uint64_t>{ 2 });
    CHECK(Ids(view.Skinned) == std::vector<uint64_t>{ 4, 7 });
    CHECK(Ids(view.SkinnedCutoff) == std::vector<uint64_t>{ 5 });
    CHECK(Ids(view.SkinnedTransparent) == std::vector<uint64_t>{ 6 });

    // Without draw info every instance is opaque, as before alpha modes existed.
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), view);
    CHECK(view.Opaque.size() == 4);
    CHECK(view.Skinned.size() == 4);
    CHECK(view.Cutoff.empty());
    CHECK(view.Transparent.empty());
}
