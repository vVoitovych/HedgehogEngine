#include "api/FontLoader.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ContentLoader
{
    namespace
    {
        constexpr uint32_t PADDING         = 1;  // empty pixels around every glyph in the atlas
        constexpr uint32_t MIN_ATLAS_SIZE  = 64;
        constexpr uint32_t FALLBACK_SYMBOL = '?';

        // A glyph to bake: its codepoint and the font's glyph index for it.
        struct GlyphSource
        {
            uint32_t Codepoint = 0;
            int      Index     = 0;
        };

        uint32_t NextPowerOfTwo(uint32_t value)
        {
            uint32_t power = 1;
            while (power < value)
                power *= 2;
            return power;
        }

        // The printable ASCII and Latin-1 codepoints the font has, then '?' or .notdef as the fallback.
        std::vector<GlyphSource> ChooseGlyphs(const stbtt_fontinfo& info, uint32_t& fallback)
        {
            std::vector<GlyphSource> sources;
            const auto addRange = [&](uint32_t first, uint32_t last)
            {
                for (uint32_t codepoint = first; codepoint <= last; ++codepoint)
                {
                    if (const int index = stbtt_FindGlyphIndex(&info, static_cast<int>(codepoint)); index != 0)
                        sources.push_back({ codepoint, index });
                }
            };
            addRange(0x20, 0x7E);
            addRange(0xA0, 0xFF);

            const auto question = std::find_if(sources.begin(), sources.end(), [](const GlyphSource& source)
                                               { return source.Codepoint == FALLBACK_SYMBOL; });
            if (question != sources.end())
            {
                fallback = static_cast<uint32_t>(question - sources.begin());
                return sources;
            }
            sources.insert(sources.begin(), GlyphSource{ 0, 0 }); // .notdef, sorted first
            fallback = 0;
            return sources;
        }

        // Places every glyph with a bitmap on shelves, tallest first, and sizes the atlas to fit:
        // a power-of-two width from the glyphs' total area, and a power-of-two height.
        void PackGlyphs(LoadedFont& font)
        {
            std::vector<uint32_t> order(font.Glyphs.size());
            std::iota(order.begin(), order.end(), 0u);
            std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b)
                             { return font.Glyphs[a].Height > font.Glyphs[b].Height; });

            uint64_t area     = 0;
            uint32_t maxWidth = 0;
            for (const LoadedGlyph& glyph : font.Glyphs)
            {
                area += static_cast<uint64_t>(glyph.Width + PADDING) * (glyph.Height + PADDING);
                maxWidth = std::max(maxWidth, glyph.Width + 2 * PADDING);
            }
            const uint32_t side  = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(area))));
            const uint32_t width = NextPowerOfTwo(std::max({ side, maxWidth, MIN_ATLAS_SIZE }));

            uint32_t x           = PADDING;
            uint32_t y           = PADDING;
            uint32_t shelfHeight = 0;
            for (const uint32_t index : order)
            {
                LoadedGlyph& glyph = font.Glyphs[index];
                if (glyph.Width == 0 || glyph.Height == 0)
                    continue;
                if (x + glyph.Width + PADDING > width)
                {
                    x = PADDING;
                    y += shelfHeight + PADDING;
                    shelfHeight = 0;
                }
                glyph.AtlasX = x;
                glyph.AtlasY = y;
                x += glyph.Width + PADDING;
                shelfHeight = std::max(shelfHeight, glyph.Height);
            }

            font.AtlasWidth  = width;
            font.AtlasHeight = NextPowerOfTwo(std::max(y + shelfHeight + PADDING, MIN_ATLAS_SIZE));
        }
    }

    std::optional<LoadedFont> LoadFont(const std::string& fileName, float pixelHeight,
                                       const FS::FileSystemManager& fileSystem)
    {
        const std::string path = FS::ToAssetVirtualPath(fileName);
        if (!(pixelHeight > 0.0f))
        {
            LOGERROR("Font", path, "cannot be baked at a pixel height of", pixelHeight);
            return std::nullopt;
        }

        const auto bytes = fileSystem.ReadFile(path);
        if (!bytes)
        {
            LOGERROR("Failed to read font file:", path);
            return std::nullopt;
        }

        const auto*    data   = reinterpret_cast<const unsigned char*>(bytes->data());
        const int      offset = bytes->size() < 12 ? -1 : stbtt_GetFontOffsetForIndex(data, 0);
        stbtt_fontinfo info;
        if (offset < 0 || stbtt_InitFont(&info, data, offset) == 0)
        {
            LOGERROR("Font file", path, "is not a TrueType font.");
            return std::nullopt;
        }

        LoadedFont font;
        font.PixelHeight  = pixelHeight;
        const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        font.Ascent  = static_cast<float>(ascent) * scale;
        font.Descent = static_cast<float>(descent) * scale;
        font.LineGap = static_cast<float>(lineGap) * scale;

        const std::vector<GlyphSource> sources = ChooseGlyphs(info, font.FallbackGlyph);
        font.Glyphs.reserve(sources.size());
        for (const GlyphSource& source : sources)
        {
            int advance = 0, leftBearing = 0;
            stbtt_GetGlyphHMetrics(&info, source.Index, &advance, &leftBearing);
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            if (stbtt_IsGlyphEmpty(&info, source.Index) == 0)
                stbtt_GetGlyphBitmapBox(&info, source.Index, scale, scale, &x0, &y0, &x1, &y1);

            LoadedGlyph glyph;
            glyph.Codepoint = source.Codepoint;
            glyph.Width     = static_cast<uint32_t>(std::max(x1 - x0, 0));
            glyph.Height    = static_cast<uint32_t>(std::max(y1 - y0, 0));
            glyph.OffsetX   = static_cast<float>(x0);
            glyph.OffsetY   = static_cast<float>(y0);
            glyph.Advance   = static_cast<float>(advance) * scale;
            font.Glyphs.push_back(glyph);
        }

        PackGlyphs(font);
        font.Atlas.assign(static_cast<size_t>(font.AtlasWidth) * font.AtlasHeight, 0);
        for (size_t i = 0; i < font.Glyphs.size(); ++i)
        {
            const LoadedGlyph& glyph = font.Glyphs[i];
            if (glyph.Width == 0 || glyph.Height == 0)
                continue;
            unsigned char* target = font.Atlas.data() + static_cast<size_t>(glyph.AtlasY) * font.AtlasWidth + glyph.AtlasX;
            stbtt_MakeGlyphBitmap(&info, target, static_cast<int>(glyph.Width), static_cast<int>(glyph.Height),
                                  static_cast<int>(font.AtlasWidth), scale, scale, sources[i].Index);
        }

        // Both the kern table and GPOS pair adjustments, through stb_truetype's one query.
        for (uint32_t left = 0; left < sources.size(); ++left)
        {
            for (uint32_t right = 0; right < sources.size(); ++right)
            {
                const int kern = stbtt_GetGlyphKernAdvance(&info, sources[left].Index, sources[right].Index);
                if (kern != 0)
                    font.Kerning.push_back({ left, right, static_cast<float>(kern) * scale });
            }
        }
        return font;
    }
}
