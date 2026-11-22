#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "HedgehogNavigation/api/NavMesh.hpp"
#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace
{
    // A 20 x 20 m floor at z = 0, baked and loaded.
    HN::NavMesh MakeFloorNavMesh()
    {
        HN::NavMeshGeometry floor;
        floor.Positions = { HM::Vector3(-10.0f, -10.0f, 0.0f), HM::Vector3(10.0f, -10.0f, 0.0f),
                            HM::Vector3(10.0f, 10.0f, 0.0f), HM::Vector3(-10.0f, 10.0f, 0.0f) };
        floor.Indices   = { 0, 1, 2, 0, 2, 3 };
        const HN::NavMeshBakeResult baked = HN::BakeNavMesh(floor, HN::NavMeshBuildSettings{});
        REQUIRE(baked.Error.empty());
        HN::NavMesh navMesh;
        REQUIRE(navMesh.Load(baked.Data));
        return navMesh;
    }
}

TEST_CASE("Navigation debug extraction - one line pair per edge, lifted, in the navmesh colour")
{
    const HN::NavMesh navMesh = MakeFloorNavMesh();
    std::vector<std::pair<HM::Vector3, HM::Vector3>> edges;
    navMesh.GetDebugEdges(edges);
    REQUIRE_FALSE(edges.empty());

    HX::RenderScene scene;
    scene.DebugLines.push_back({ HM::Vector3(), 0u }); // lines already there are kept
    scene.DebugLines.push_back({ HM::Vector3(), 0u });
    HX::SceneExtractor{}.ExtractNavigationDebug(navMesh, scene);
    REQUIRE(scene.DebugLines.size() == 2 + edges.size() * 2);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        const HX::DebugLineVertex& a = scene.DebugLines[2 + i * 2];
        const HX::DebugLineVertex& b = scene.DebugLines[3 + i * 2];
        CHECK(a.Color == HX::SceneExtractor::NAVMESH_DEBUG_COLOR);
        CHECK(b.Color == HX::SceneExtractor::NAVMESH_DEBUG_COLOR);
        CHECK(std::abs(a.Position.x() - edges[i].first.x()) < 1e-5f);
        CHECK(std::abs(a.Position.z() - edges[i].first.z() - HX::SceneExtractor::NAVMESH_DEBUG_LIFT) < 1e-5f);
        CHECK(std::abs(b.Position.y() - edges[i].second.y()) < 1e-5f);
    }
}

TEST_CASE("Navigation debug extraction - nothing without a navmesh")
{
    const HN::NavMesh navMesh;
    HX::RenderScene   scene;
    HX::SceneExtractor{}.ExtractNavigationDebug(navMesh, scene);
    CHECK(scene.DebugLines.empty());
}
