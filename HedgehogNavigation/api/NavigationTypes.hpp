#pragma once

#include "HedgehogNavigationApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Navigation data the engine hands in, plain data only: no Recast or Detour type is ever named here
// (module boundary rule 6). Distances in metres, angles in degrees; the world is Z-up.
namespace HN
{
    // How a navmesh is built, Recast's rcConfig in world units. The defaults suit a human-sized
    // agent on a level a few hundred metres across.
    struct NavMeshBuildSettings
    {
        float    CellSize             = 0.3f;  // the voxel grid's horizontal cell
        float    CellHeight           = 0.2f;  // and its vertical step
        float    AgentRadius          = 0.4f;  // walkable area kept this far from walls
        float    AgentHeight          = 1.8f;  // the clearance a walkable span needs above it
        float    AgentMaxClimb        = 0.4f;  // the highest ledge an agent steps up
        float    AgentMaxSlope        = 45.0f; // the steepest walkable slope
        float    RegionMinSize        = 8.0f;  // islands smaller than this many cells squared are dropped
        float    RegionMergeSize      = 20.0f; // regions smaller than this many cells squared are merged
        float    EdgeMaxLength        = 12.0f; // contour edges are split past this length
        float    EdgeMaxError         = 1.3f;  // how far a simplified contour may stray from the raw one, in cells
        int32_t  VertsPerPoly         = 6;     // at most 6 (Detour's DT_VERTS_PER_POLYGON)
        float    DetailSampleDistance = 6.0f;  // the detail mesh's sampling step, in cells
        float    DetailSampleMaxError = 1.0f;  // how far the detail mesh may stray from the surface, in cell heights
    };

    // The walkable geometry a navmesh is built from: triangles in world space, Z-up, wound
    // counterclockwise seen from above (the up side walkable).
    struct NavMeshGeometry
    {
        std::vector<HM::Vector3> Positions;
        std::vector<uint32_t>    Indices; // three per triangle
    };

    // Whether Recast and Detour are linked and work: builds a tiny heightfield and allocates a navmesh,
    // empty on success, else what failed. For the module's tests until the builder lands.
    [[nodiscard]] HEDGEHOG_NAVIGATION_API std::string CheckNavigationLibraries();
}
