#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// What a terrain tile's heights mean as a surface, in the tile's local space (x and y in
// [0, Size], height along +Z). Each grid cell (column c, row r) is two triangles split along its
// diagonal from (c, r) to (c + 1, r + 1): (c, r), (c + 1, r), (c + 1, r + 1) and (c, r),
// (c + 1, r + 1), (c, r + 1), counterclockwise seen from above. BuildTerrainMesh, SampleTerrainHeight
// and RaycastTerrain all follow these triangles, so a query answers what is drawn. A heights grid
// of the wrong length reads as flat at 0.
namespace HedgehogEngine
{
    struct TerrainMesh
    {
        std::vector<HM::Vector3> Positions; // Resolution^2, row-major as TerrainData::Heights
        std::vector<HM::Vector2> TexCoords; // 0 to 1 across the tile, u along x, v along y
        std::vector<HM::Vector3> Normals;   // from central differences of the heights
        std::vector<HM::Vector4> Tangents;  // along +x, orthogonal to the normal; w +1
        std::vector<uint32_t>    Indices;   // 6 per cell
    };

    struct TerrainRayHit
    {
        HM::Vector3 Point;
        HM::Vector3 Normal;   // the hit triangle's, facing up
        float       Distance; // along the ray, in the units of its normalized direction
    };

    // The distance between neighbouring height samples.
    [[nodiscard]] HEDGEHOG_ENGINE_API float GetTerrainSpacing(const TerrainData& data);

    [[nodiscard]] HEDGEHOG_ENGINE_API TerrainMesh BuildTerrainMesh(const TerrainData& data);

    // The surface's height at (x, y), clamped to the tile's edges.
    [[nodiscard]] HEDGEHOG_ENGINE_API float SampleTerrainHeight(const TerrainData& data, float x, float y);

    // The nearest point where a ray from origin along direction (any non-zero length) meets the
    // surface within maxDistance; nothing for a miss, a zero direction or a maxDistance that is not
    // positive. Rays meet the triangles from either side.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<TerrainRayHit> RaycastTerrain(const TerrainData& data,
                                                                                  const HM::Vector3& origin,
                                                                                  const HM::Vector3& direction,
                                                                                  float              maxDistance);
}
