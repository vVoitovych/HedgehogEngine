#pragma once

#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <vector>

// The terrain sculpting brush, ImGui-free: what one frame of a stroke does to a tile's heights.
// Positions are in the tile's local space (x and y in [0, Size]), as RaycastTerrain gives them.
namespace Editor
{
    enum class TerrainBrushTool
    {
        Raise,   // up by Strength metres a second at the centre
        Lower,   // down likewise
        Smooth,  // towards the average of each sample's neighbours
        Flatten, // towards FlattenHeight, the height where the stroke began
    };

    inline constexpr const char* TERRAIN_BRUSH_TOOL_NAMES[] = { "Raise", "Lower", "Smooth", "Flatten" };

    struct TerrainBrush
    {
        TerrainBrushTool Tool          = TerrainBrushTool::Raise;
        float            Radius        = 8.0f; // metres
        float            Strength      = 4.0f; // metres a second for Raise and Lower; how fast the others pull
        float            Falloff       = 0.5f; // the outer fraction of the radius that fades to nothing
        float            FlattenHeight = 0.0f;
    };

    // The brush's weight at distance from its centre: 1 within (1 - falloff) of the radius, easing
    // smoothly to 0 at the radius, 0 beyond it.
    [[nodiscard]] float GetBrushWeight(float distance, float radius, float falloff);

    // One frame of a stroke at (x, y) lasting dt seconds: every sample within the radius moves by its
    // weight, the heights kept in [0, HeightScale]. Whether any height changed.
    bool ApplyTerrainBrush(HedgehogEngine::TerrainData& data, const TerrainBrush& brush, float x, float y, float dt);

    // The brush's outline on the surface: segments points around (x, y) at the radius, each at the
    // surface's height (SampleTerrainHeight), the last joining the first.
    [[nodiscard]] std::vector<HM::Vector3> MakeBrushRing(const HedgehogEngine::TerrainData& data, float x, float y,
                                                         float radius, uint32_t segments);
}
