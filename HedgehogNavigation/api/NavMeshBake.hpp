#pragma once

#include "HedgehogNavigationApi.hpp"
#include "NavigationTypes.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Baking a navmesh (Recast's single-tile pipeline) and reading one back, through plain data only.
namespace HN
{
    struct NavMeshBakeResult
    {
        std::vector<uint8_t> Data;  // Detour navmesh data, empty on error
        std::string          Error; // one message naming the input or the stage at fault
        uint32_t             PolygonCount = 0;
    };

    // Bakes geometry into a Detour navmesh of one tile: a heightfield over its bounds, the triangles no
    // steeper than AgentMaxSlope rasterized as walkable, the span filters, erosion by the agent radius,
    // watershed regions, contours, polygons and the detail mesh. Refuses, naming why, geometry without
    // triangles, indices not a whole number of triangles, an index past the positions, a position that
    // is not finite, and settings out of range (a cell, the agent's height, a slope or a region size
    // that is not positive, a negative radius, climb or edge, 3 to 6 vertices per polygon), before
    // Recast runs. The same input bakes the same bytes. Recast's own messages are logged as
    // [Navigation] ... .
    [[nodiscard]] HEDGEHOG_NAVIGATION_API NavMeshBakeResult BakeNavMesh(const NavMeshGeometry& geometry,
                                                                        const NavMeshBuildSettings& settings);

    // A baked navmesh as the engine sees it, Z-up: each polygon's centre and the island (connected
    // set of polygons an agent can walk between) it belongs to, numbered from 0 in polygon order.
    struct NavMeshDescription
    {
        std::vector<HM::Vector3> PolygonCentres;
        std::vector<uint32_t>    Islands;
        uint32_t                 IslandCount = 0;
    };

    // Reads data BakeNavMesh made; nullopt when Detour does not accept it.
    [[nodiscard]] HEDGEHOG_NAVIGATION_API std::optional<NavMeshDescription> DescribeNavMesh(const std::vector<uint8_t>& data);
}
