#pragma once

#include "ContentLoader/api/LoadedFont.hpp"
#include "HedgehogExtract/api/UiDrawList.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// Laying text out with a baked ContentLoader::LoadedFont, as free functions. Text is UTF-8; a
// codepoint the font was not baked with, or a malformed byte, draws the font's fallback glyph.
namespace HUI
{
    enum class TextAlign
    {
        Left,
        Center,
        Right,
    };

    enum class TextVerticalAlign
    {
        Top,
        Middle,
        Bottom,
    };

    struct TextStyle
    {
        TextAlign         Align         = TextAlign::Left;
        TextVerticalAlign VerticalAlign = TextVerticalAlign::Top;
        bool              Wrap          = true;       // break lines at word boundaries to fit the rect
        float             LineSpacing   = 1.0f;       // a multiple of the font's line height
        uint32_t          Color         = 0xFFFFFFFF; // PackColor's RGBA8
        uint32_t          Texture       = HX::UI_NO_TEXTURE; // the font atlas
    };

    // One line of text: bytes [Begin, End) of the string, without the spaces or newline that end it.
    struct TextLine
    {
        size_t Begin = 0;
        size_t End   = 0;
        float  Width = 0.0f; // advances and kerning of its glyphs, in pixels
    };

    // The glyph for codepoint, or the font's fallback glyph; nullptr only for a font with no glyphs.
    [[nodiscard]] const ContentLoader::LoadedGlyph* FindGlyph(const ContentLoader::LoadedFont& font, uint32_t codepoint);

    // The kerning between two glyphs, by their indices in font.Glyphs; 0 for a pair without any.
    [[nodiscard]] float GetKerning(const ContentLoader::LoadedFont& font, uint32_t left, uint32_t right);

    // The distance between two lines' baselines at style's LineSpacing.
    [[nodiscard]] float GetLineHeight(const ContentLoader::LoadedFont& font, float lineSpacing = 1.0f);

    // Splits text into lines: at every '\n' and, with wrap, before a word that would end past
    // maxWidth. A word wider than maxWidth on its own breaks between characters, at least one per
    // line. Spaces where a wrapped line breaks belong to no line. Replaces lines' contents.
    void BreakLines(const ContentLoader::LoadedFont& font, std::string_view text, float maxWidth, bool wrap,
                    std::vector<TextLine>& lines);

    // Appends text's glyph quads to list, laid out in rect (pixels) by style: lines broken as
    // BreakLines does at rect's width, each aligned in rect horizontally, the block of lines
    // vertically. Line origins snap to whole pixels. Glyphs are clipped to rect inside scissor, so
    // overflowing text is cut off at rect's edges. Allocates only as the list grows.
    void LayoutText(HX::UiDrawList& list, const ContentLoader::LoadedFont& font, std::string_view text,
                    const HX::UiRect& rect, const TextStyle& style, const HX::UiRect& scissor);
}
