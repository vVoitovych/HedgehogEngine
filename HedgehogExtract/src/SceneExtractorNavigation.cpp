#include "api/SceneExtractor.hpp"

#include "api/RenderScene.hpp"

#include "HedgehogNavigation/api/NavMesh.hpp"

#include <utility>
#include <vector>

namespace HX
{
    void SceneExtractor::ExtractNavigationDebug(const HN::NavMesh& navMesh, RenderScene& outScene) const
    {
        // Kept between calls, so a frame allocates nothing once it has grown.
        static thread_local std::vector<std::pair<HM::Vector3, HM::Vector3>> edges;
        navMesh.GetDebugEdges(edges);
        const HM::Vector3 lift(0.0f, 0.0f, NAVMESH_DEBUG_LIFT);
        for (const auto& [a, b] : edges)
        {
            outScene.DebugLines.push_back({ a + lift, NAVMESH_DEBUG_COLOR });
            outScene.DebugLines.push_back({ b + lift, NAVMESH_DEBUG_COLOR });
        }
    }
}
