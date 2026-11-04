#include "TerrainEditing.hpp"

#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Editor
{
    bool ResampleTerrain(HedgehogEngine::TerrainData& data, uint32_t resolution)
    {
        if (!HedgehogEngine::IsValidTerrainResolution(resolution))
            return false;
        if (resolution == data.Resolution)
            return true;

        const float        spacing = data.Size / static_cast<float>(resolution - 1);
        std::vector<float> heights(static_cast<size_t>(resolution) * resolution);
        for (uint32_t row = 0; row < resolution; ++row)
            for (uint32_t column = 0; column < resolution; ++column)
                heights[static_cast<size_t>(row) * resolution + column] =
                    HedgehogEngine::SampleTerrainHeight(data, column * spacing, row * spacing);
        data.Resolution = resolution;
        data.Heights    = std::move(heights);
        return true;
    }

    bool SetTerrainHeightScale(HedgehogEngine::TerrainData& data, float heightScale)
    {
        if (!std::isfinite(heightScale) || heightScale <= 0.0f)
            return false;
        data.HeightScale = heightScale;
        for (float& height : data.Heights)
            height = std::clamp(height, 0.0f, heightScale);
        return true;
    }
}
