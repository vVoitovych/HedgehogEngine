#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// One terrain tile, Z-up: a square of Size metres over its entity's local x and y in [0, Size],
// sampled by a Resolution x Resolution grid of heights (row-major, row 0 at y = 0, column 0 at
// x = 0), each in metres from 0 to HeightScale. Four material layers are blended by the splat map,
// a SplatResolution x SplatResolution grid of RGBA8 weights laid out as the heights (R layer 0 ...
// A layer 3).
//
// A .terrain file, version 1:
//
//   Version: 1
//   Size: 128
//   Resolution: 129
//   HeightScale: 64
//   SplatResolution: 256
//   Layers:
//     - BaseColor: Textures/Grass.png
//       NormalMap: Textures/Grass_Normal.png
//       Tiling: 16
//       Metallic: 0
//       Roughness: 1
//     - ... (exactly four)
//   Heights: <base64 of Resolution^2 little-endian uint16, 0 to 65535 over 0 to HeightScale>
//   Splat: <base64 of SplatResolution^2 RGBA8 texels>
//
// Every key is required, so what WriteTerrain writes ParseTerrain reads back, and writing that
// again gives the same text. Layer maps are paths under assets:// (without the prefix, as
// materials name theirs), empty for none.
namespace HedgehogEngine
{
    inline constexpr int              TERRAIN_FILE_VERSION = 1;
    inline constexpr std::string_view TERRAIN_EXTENSION    = ".terrain";

    inline constexpr uint32_t TERRAIN_LAYER_COUNT       = 4;
    inline constexpr uint32_t MIN_TERRAIN_RESOLUTION    = 33;
    inline constexpr uint32_t MAX_TERRAIN_RESOLUTION    = 1025;
    inline constexpr uint32_t MIN_SPLAT_RESOLUTION      = 16;
    inline constexpr uint32_t MAX_SPLAT_RESOLUTION      = 4096;
    inline constexpr float    MAX_TERRAIN_SIZE          = 100000.0f;
    inline constexpr float    DEFAULT_TERRAIN_SIZE      = 128.0f;
    inline constexpr uint32_t DEFAULT_TERRAIN_RESOLUTION = 129;
    inline constexpr float    DEFAULT_HEIGHT_SCALE      = 64.0f;
    inline constexpr uint32_t DEFAULT_SPLAT_RESOLUTION  = 256;
    inline constexpr float    DEFAULT_LAYER_TILING      = 16.0f;

    struct TerrainLayer
    {
        std::string BaseColor; // under assets://, empty for white
        std::string NormalMap; // under assets://, empty for flat
        float       Tiling    = DEFAULT_LAYER_TILING; // repeats of the maps across the tile
        float       Metallic  = 0.0f;
        float       Roughness = 1.0f;

        bool operator==(const TerrainLayer&) const = default;
    };

    struct TerrainData
    {
        float                                          Size            = DEFAULT_TERRAIN_SIZE;
        uint32_t                                       Resolution      = DEFAULT_TERRAIN_RESOLUTION;
        float                                          HeightScale     = DEFAULT_HEIGHT_SCALE;
        uint32_t                                       SplatResolution = DEFAULT_SPLAT_RESOLUTION;
        std::array<TerrainLayer, TERRAIN_LAYER_COUNT>  Layers;
        std::vector<float>                             Heights; // Resolution^2 metres
        std::vector<uint8_t>                           Splat;   // SplatResolution^2 * 4 weights

        bool operator==(const TerrainData&) const = default;
    };

    struct TerrainReadResult
    {
        std::optional<TerrainData> Data;  // empty on any error
        std::string                Error; // one message naming the entry at fault
    };

    // Whether a height grid of resolution x resolution samples is allowed: 2^n + 1 from
    // MIN_TERRAIN_RESOLUTION to MAX_TERRAIN_RESOLUTION, so the grid halves evenly.
    [[nodiscard]] HEDGEHOG_ENGINE_API bool IsValidTerrainResolution(uint32_t resolution);
    // Whether a splat grid of resolution x resolution texels is allowed: a power of two from
    // MIN_SPLAT_RESOLUTION to MAX_SPLAT_RESOLUTION.
    [[nodiscard]] HEDGEHOG_ENGINE_API bool IsValidSplatResolution(uint32_t resolution);

    // A flat tile at height 0 of the default size and resolutions, painted entirely with layer 0.
    [[nodiscard]] HEDGEHOG_ENGINE_API TerrainData MakeDefaultTerrain();

    // Reads text strictly: malformed YAML, a missing or other Version, an unknown or missing key,
    // a value that is not a finite number, a size or height scale that is not positive (or a size
    // past MAX_TERRAIN_SIZE), a resolution the checks above refuse, not exactly four layers, and
    // Heights or Splat that is not base64 of exactly the grid's bytes each fail with one message.
    [[nodiscard]] HEDGEHOG_ENGINE_API TerrainReadResult ParseTerrain(std::string_view text);

    // Writes data in the layout above, heights clamped to [0, HeightScale] and quantized to 16
    // bits. A grid of the wrong length is written as a flat or layer-0 grid of the right one.
    [[nodiscard]] HEDGEHOG_ENGINE_API std::string WriteTerrain(const TerrainData& data);
}
