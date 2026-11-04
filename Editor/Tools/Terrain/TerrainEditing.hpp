#pragma once

#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include <array>
#include <cstdint>

// Inspector edits of a terrain tile, ImGui-free.
namespace Editor
{
    // The resolutions the inspector offers, every one IsValidTerrainResolution allows.
    inline constexpr std::array<uint32_t, 6> TERRAIN_RESOLUTIONS = { 33, 65, 129, 257, 513, 1025 };

    // Gives data a grid of resolution x resolution heights over the same surface: each new sample
    // is the old surface's height at its place (SampleTerrainHeight), so the shape is kept. False,
    // changing nothing, for a resolution IsValidTerrainResolution refuses.
    bool ResampleTerrain(HedgehogEngine::TerrainData& data, uint32_t resolution);

    // Changes the height range, keeping every height in metres where it fits and clamping the rest
    // into [0, heightScale]. False, changing nothing, for a scale that is not positive and finite.
    bool SetTerrainHeightScale(HedgehogEngine::TerrainData& data, float heightScale);
}
