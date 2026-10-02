#include "HedgehogUI/api/TextLayout.hpp"

#include "HedgehogUI/api/UiDraw.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace HUI
{
    namespace
    {
        constexpr uint32_t REPLACEMENT_CHARACTER = 0xFFFD;
        constexpr float    WIDTH_TOLERANCE       = 1e-3f;

        // Decodes the UTF-8 codepoint at pos and moves pos past it. A malformed sequence decodes
        // as U+FFFD and moves on by one byte.
        uint32_t DecodeUtf8(std::string_view text, size_t& pos)
        {
            const auto lead = static_cast<unsigned char>(text[pos]);
            if (lead < 0x80)
            {
                ++pos;
                return lead;
            }

            size_t   length    = 0;
            uint32_t codepoint = 0;
            uint32_t minimum   = 0;
            if ((lead & 0xE0) == 0xC0)
                length = 2, codepoint = lead & 0x1Fu, minimum = 0x80;
            else if ((lead & 0xF0) == 0xE0)
                length = 3, codepoint = lead & 0x0Fu, minimum = 0x800;
            else if ((lead & 0xF8) == 0xF0)
                length = 4, codepoint = lead & 0x07u, minimum = 0x10000;

            if (length == 0 || pos + length > text.size())
            {
                ++pos;
                return REPLACEMENT_CHARACTER;
            }
            for (size_t i = 1; i < length; ++i)
            {
                const auto next = static_cast<unsigned char>(text[pos + i]);
                if ((next & 0xC0) != 0x80)
                {
                    ++pos;
                    return REPLACEMENT_CHARACTER;
                }
                codepoint = (codepoint << 6) | (next & 0x3Fu);
            }
            if (codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            {
                ++pos;
                return REPLACEMENT_CHARACTER;
            }
            pos += length;
            return codepoint;
        }

        uint32_t IndexOf(const ContentLoader::LoadedFont& font, const ContentLoader::LoadedGlyph* glyph)
        {
            return static_cast<uint32_t>(glyph - font.Glyphs.data());
        }

        // The kerning to add before glyph when previous (the glyph before it, or nullptr at a line's start)
        // comes before it.
        float KerningBefore(const ContentLoader::LoadedFont& font, const ContentLoader::LoadedGlyph* previous,
                            const ContentLoader::LoadedGlyph& glyph)
        {
            return previous != nullptr ? GetKerning(font, IndexOf(font, previous), IndexOf(font, &glyph)) : 0.0f;
        }

        struct LineBreak
        {
            TextLine Line;
            size_t   Next    = 0;     // where the following line starts
            bool     Newline = false; // the line ended at a '\n'
        };

        // The line starting at start; see BreakLines.
        LineBreak NextLine(const ContentLoader::LoadedFont& font, std::string_view text, size_t start, float maxWidth,
                           bool wrap)
        {
            LineBreak result;
            result.Line = { start, start, 0.0f };

            float                             penX     = 0.0f;
            const ContentLoader::LoadedGlyph* previous = nullptr;
            size_t contentEnd   = start; // just past the last character that is not a space
            float  contentWidth = 0.0f;
            size_t wordStart    = start; // the first character of the current word
            size_t breakEnd     = start; // where the line ends if it wraps before the current word
            float  breakWidth   = 0.0f;
            bool   inSpaces     = false;

            size_t pos = start;
            while (pos < text.size())
            {
                const size_t   characterStart = pos;
                const uint32_t codepoint      = DecodeUtf8(text, pos);
                if (codepoint == '\n')
                {
                    result.Line    = { start, contentEnd, contentWidth };
                    result.Next    = pos;
                    result.Newline = true;
                    return result;
                }
                if (codepoint == '\r')
                    continue;
                const ContentLoader::LoadedGlyph* glyph = FindGlyph(font, codepoint);
                if (glyph == nullptr)
                    continue;

                if (codepoint == ' ')
                {
                    if (!inSpaces && contentEnd > start)
                    {
                        breakEnd   = contentEnd;
                        breakWidth = contentWidth;
                    }
                    inSpaces = true;
                    penX += KerningBefore(font, previous, *glyph) + glyph->Advance;
                    previous = glyph;
                    continue;
                }
                if (inSpaces)
                    wordStart = characterStart;
                inSpaces = false;

                const float end = penX + KerningBefore(font, previous, *glyph) + glyph->Advance;
                if (wrap && end > maxWidth + WIDTH_TOLERANCE && contentEnd > start)
                {
                    if (breakEnd > start)
                    {
                        result.Line = { start, breakEnd, breakWidth };
                        result.Next = wordStart;
                    }
                    else // one word wider than the line: break between its characters
                    {
                        result.Line = { start, contentEnd, contentWidth };
                        result.Next = characterStart;
                    }
                    return result;
                }
                penX         = end;
                previous     = glyph;
                contentEnd   = pos;
                contentWidth = penX;
            }

            result.Line = { start, contentEnd, contentWidth };
            result.Next = text.size();
            return result;
        }

        // Calls visit(line) for every line of text, as BreakLines lists them.
        template<typename Visit>
        void ForEachLine(const ContentLoader::LoadedFont& font, std::string_view text, float maxWidth, bool wrap,
                         Visit&& visit)
        {
            size_t start = 0;
            while (start < text.size())
            {
                const LineBreak lineBreak = NextLine(font, text, start, maxWidth, wrap);
                visit(lineBreak.Line);
                if (lineBreak.Newline && lineBreak.Next >= text.size())
                    visit(TextLine{ text.size(), text.size(), 0.0f });
                start = lineBreak.Next;
            }
        }

        HX::UiRect Intersect(const HX::UiRect& a, const HX::UiRect& b)
        {
            const float left   = std::max(a.X, b.X);
            const float top    = std::max(a.Y, b.Y);
            const float right  = std::min(a.X + a.Width, b.X + b.Width);
            const float bottom = std::min(a.Y + a.Height, b.Y + b.Height);
            return { left, top, std::max(right - left, 0.0f), std::max(bottom - top, 0.0f) };
        }
    }

    const ContentLoader::LoadedGlyph* FindGlyph(const ContentLoader::LoadedFont& font, uint32_t codepoint)
    {
        if (font.Glyphs.empty())
            return nullptr;
        const auto found = std::lower_bound(font.Glyphs.begin(), font.Glyphs.end(), codepoint,
                                            [](const ContentLoader::LoadedGlyph& glyph, uint32_t value)
                                            { return glyph.Codepoint < value; });
        if (found != font.Glyphs.end() && found->Codepoint == codepoint)
            return &*found;
        return &font.Glyphs[std::min<size_t>(font.FallbackGlyph, font.Glyphs.size() - 1)];
    }

    float GetKerning(const ContentLoader::LoadedFont& font, uint32_t left, uint32_t right)
    {
        const auto found = std::lower_bound(font.Kerning.begin(), font.Kerning.end(), std::make_pair(left, right),
                                            [](const ContentLoader::LoadedKerningPair& pair,
                                               const std::pair<uint32_t, uint32_t>& value)
                                            { return std::make_pair(pair.Left, pair.Right) < value; });
        if (found != font.Kerning.end() && found->Left == left && found->Right == right)
            return found->Advance;
        return 0.0f;
    }

    float GetLineHeight(const ContentLoader::LoadedFont& font, float lineSpacing)
    {
        return (font.Ascent - font.Descent + font.LineGap) * lineSpacing;
    }

    void BreakLines(const ContentLoader::LoadedFont& font, std::string_view text, float maxWidth, bool wrap,
                    std::vector<TextLine>& lines)
    {
        lines.clear();
        ForEachLine(font, text, maxWidth, wrap, [&](const TextLine& line) { lines.push_back(line); });
    }

    void LayoutText(HX::UiDrawList& list, const ContentLoader::LoadedFont& font, std::string_view text,
                    const HX::UiRect& rect, const TextStyle& style, const HX::UiRect& scissor)
    {
        const HX::UiRect clip = Intersect(rect, scissor);
        if (font.Glyphs.empty() || clip.Width <= 0.0f || clip.Height <= 0.0f)
            return;

        size_t lineCount = 0;
        ForEachLine(font, text, rect.Width, style.Wrap, [&](const TextLine&) { ++lineCount; });
        if (lineCount == 0)
            return;

        // The block runs from the first line's ascent to the last line's descent.
        const float lineHeight  = GetLineHeight(font, style.LineSpacing);
        const float blockHeight = static_cast<float>(lineCount - 1) * lineHeight + font.Ascent - font.Descent;
        float       top         = rect.Y;
        if (style.VerticalAlign == TextVerticalAlign::Middle)
            top += (rect.Height - blockHeight) * 0.5f;
        else if (style.VerticalAlign == TextVerticalAlign::Bottom)
            top += rect.Height - blockHeight;

        const float atlasWidth  = static_cast<float>(std::max(font.AtlasWidth, 1u));
        const float atlasHeight = static_cast<float>(std::max(font.AtlasHeight, 1u));
        UiQuad      quad;
        quad.Color   = style.Color;
        quad.Texture = style.Texture;

        size_t lineIndex = 0;
        ForEachLine(font, text, rect.Width, style.Wrap,
                    [&](const TextLine& line)
                    {
                        float left = rect.X;
                        if (style.Align == TextAlign::Center)
                            left += (rect.Width - line.Width) * 0.5f;
                        else if (style.Align == TextAlign::Right)
                            left += rect.Width - line.Width;
                        const float originX  = std::round(left);
                        const float baseline = std::round(top + static_cast<float>(lineIndex) * lineHeight + font.Ascent);
                        ++lineIndex;

                        float                             penX     = 0.0f;
                        const ContentLoader::LoadedGlyph* previous = nullptr;
                        size_t                            pos      = line.Begin;
                        while (pos < line.End)
                        {
                            const uint32_t codepoint = DecodeUtf8(text, pos);
                            if (codepoint == '\r')
                                continue;
                            const ContentLoader::LoadedGlyph& glyph = *FindGlyph(font, codepoint);
                            penX += KerningBefore(font, previous, glyph);
                            const float x = originX + penX;
                            quad.Rect = { x + glyph.OffsetX, baseline + glyph.OffsetY, static_cast<float>(glyph.Width),
                                          static_cast<float>(glyph.Height) };
                            quad.Uv   = { static_cast<float>(glyph.AtlasX) / atlasWidth,
                                          static_cast<float>(glyph.AtlasY) / atlasHeight,
                                          static_cast<float>(glyph.Width) / atlasWidth,
                                          static_cast<float>(glyph.Height) / atlasHeight };
                            AppendQuad(list, quad, clip);
                            penX += glyph.Advance;
                            previous = &glyph;
                        }
                    });
    }
}
