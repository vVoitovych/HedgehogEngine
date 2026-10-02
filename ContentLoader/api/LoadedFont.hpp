#pragma once

#include <cstdint>
#include <vector>

// A font baked at one pixel height: an R8 coverage atlas and the metrics to lay text out with it.
// Plain data, so a library that only lays text out (HedgehogUI) reads it through this header alone.
namespace ContentLoader
{
    // One baked glyph. Offsets and advances are in pixels; y grows down, from the baseline.
    struct LoadedGlyph
    {
        uint32_t Codepoint = 0;    // 0 for the font's own missing-glyph shape (.notdef)
        uint32_t AtlasX    = 0;    // the glyph's pixels in the atlas
        uint32_t AtlasY    = 0;
        uint32_t Width     = 0;    // 0 for a glyph with no pixels, such as a space
        uint32_t Height    = 0;
        float    OffsetX   = 0.0f; // from the pen position to the bitmap's left edge
        float    OffsetY   = 0.0f; // from the baseline to the bitmap's top edge (negative above it)
        float    Advance   = 0.0f; // how far the pen moves after this glyph
    };

    // An adjustment to the advance between two glyphs, by their indices in LoadedFont::Glyphs.
    struct LoadedKerningPair
    {
        uint32_t Left    = 0;
        uint32_t Right   = 0;
        float    Advance = 0.0f; // added to Left's advance when Right follows it (usually negative)
    };

    struct LoadedFont
    {
        float PixelHeight = 0.0f;
        float Ascent      = 0.0f; // baseline to the top of the tallest glyph, positive
        float Descent     = 0.0f; // baseline to the bottom of the lowest glyph, negative
        float LineGap     = 0.0f; // extra space between one line's descent and the next one's ascent

        uint32_t             AtlasWidth  = 0;
        uint32_t             AtlasHeight = 0;
        std::vector<uint8_t> Atlas; // AtlasWidth * AtlasHeight coverage values, row by row

        std::vector<LoadedGlyph>       Glyphs;  // sorted by Codepoint
        std::vector<LoadedKerningPair> Kerning; // sorted by Left, then Right; nonzero pairs only

        // The index in Glyphs drawn for a codepoint the font was not baked with: '?' when the font
        // has it, else the font's .notdef shape.
        uint32_t FallbackGlyph = 0;
    };
}
