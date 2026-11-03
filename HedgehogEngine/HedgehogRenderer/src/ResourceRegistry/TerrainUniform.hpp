#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <vector>

namespace HR
{
    // The terrain set (set 1 of the forward pass's terrain pipeline): binding 0 the TerrainUniform,
    // binding 1 the splat map (linear RGBA8, a weight per layer), then each layer's base colour map
    // (sRGB) at TERRAIN_BASE_COLOR_BINDING + layer and normal map (linear) at
    // TERRAIN_NORMAL_BINDING + layer, every map at its neutral default when the layer names none.
    inline constexpr uint32_t TERRAIN_SPLAT_BINDING      = 1;
    inline constexpr uint32_t TERRAIN_BASE_COLOR_BINDING = 2;
    inline constexpr uint32_t TERRAIN_NORMAL_BINDING     = TERRAIN_BASE_COLOR_BINDING + HedgehogEngine::TERRAIN_LAYER_VIEW_COUNT;
    inline constexpr uint32_t TERRAIN_BINDING_COUNT      = TERRAIN_NORMAL_BINDING + HedgehogEngine::TERRAIN_LAYER_VIEW_COUNT;

    // A TerrainLayerUniform's Flags bits: the layer names that map.
    inline constexpr uint32_t TERRAIN_LAYER_BASE_COLOR_FLAG = 1u << 0;
    inline constexpr uint32_t TERRAIN_LAYER_NORMAL_FLAG     = 1u << 1;

    struct TerrainLayerUniform
    {
        float    Tiling;    // offset 0
        float    Metallic;  // offset 4
        float    Roughness; // offset 8
        uint32_t Flags;     // offset 12
    };

    // Binding 0 of the terrain set, std140.
    struct TerrainUniform
    {
        TerrainLayerUniform Layers[HedgehogEngine::TERRAIN_LAYER_VIEW_COUNT];
    };
    static_assert(sizeof(TerrainUniform) == 64, "TerrainUniform must match the terrain shader's std140 block");

    // Non-finite values as their defaults, metallic and roughness clamped to [0, 1].
    [[nodiscard]] TerrainUniform MakeTerrainUniform(const HedgehogEngine::TerrainView& terrain);

    // The terrain set's layout, fragment stage only.
    [[nodiscard]] std::vector<RHI::DescriptorBinding> MakeTerrainSetBindings();
}
