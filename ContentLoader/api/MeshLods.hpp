#pragma once

#include "ContentLoaderApi.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <span>
#include <vector>

// Levels of detail of a mesh as simplified index lists over its own vertices (meshoptimizer), so a
// level is just another index range: no new vertex data, and a skinned mesh's levels skin unchanged.
namespace ContentLoader
{
    // A mesh below this many triangles gets no levels: drawing it whole costs little.
    inline constexpr size_t MIN_LOD_TRIANGLES = 1024;

    struct MeshLodDesc
    {
        // At most this many levels, each targeting half the triangles of the one before (50 %, 25 %,
        // 12.5 % of the mesh).
        uint32_t MaxLevels = 3;
        // The largest deviation a level may have, relative to the mesh's extent.
        float MaxError = 0.05f;
        // A level that keeps more than this fraction of the triangles of the one before is dropped,
        // and every level after it.
        float MinShrink = 0.8f;
    };

    // The levels after the mesh itself, coarsest last, each a triangle list indexing positions. None
    // for a mesh under MIN_LOD_TRIANGLES, an index count that is not a multiple of 3, or an index out
    // of range. Deterministic: the same mesh gives the same levels.
    CONTENT_LOADER_API std::vector<std::vector<uint32_t>> GenerateMeshLods(std::span<const HM::Vector3> positions,
                                                                           std::span<const uint32_t>    indices,
                                                                           const MeshLodDesc&           desc = {});
}
