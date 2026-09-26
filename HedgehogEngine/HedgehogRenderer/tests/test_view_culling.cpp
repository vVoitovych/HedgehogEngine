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
