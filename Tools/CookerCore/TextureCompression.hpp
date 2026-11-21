#pragma once

#include "ContentLoader/api/CompressedTexture.hpp"

#include <cstdint>
#include <optional>
#include <span>

// Block compression for the cook: an RGBA8 image and its CPU mips encoded into a
// ContentLoader::CompressedTexture the renderer uploads as it is. Only the tools link it (bc7enc),
// never the game.
namespace Cooker
{
    // BC7's encoder settings, fixed: every partition of mode 1, no uber search (level 0). A 2K colour
    // map takes about 0.6 s in Release (PERFORMANCE.md).
    inline constexpr uint32_t BC7_MAX_PARTITIONS = 64;
    inline constexpr uint32_t BC7_UBER_LEVEL     = 0;

    // Encodes width x height RGBA8 texels (rgba.size() == width * height * 4, rows top first) with a
    // full mip chain down to 1 x 1, each level a 2 x 2 box filter of the one before (an odd edge's
    // last texel taken twice): in linear light for Color (alpha linear), on the bytes for Data, and
    // on the decoded vectors made unit again for Normal. Blocks past an edge repeat its texels.
    // format defaults to GetEncodingFormat(encoding); Data may also take BC4Unorm (red only).
    // nullopt for no texels, a size mismatch or a format the encoding does not take.
    [[nodiscard]] std::optional<ContentLoader::CompressedTexture> CompressTexture(
        std::span<const uint8_t> rgba, uint32_t width, uint32_t height, ContentLoader::TextureEncoding encoding,
        std::optional<ContentLoader::CompressedFormat> format = std::nullopt);
}
