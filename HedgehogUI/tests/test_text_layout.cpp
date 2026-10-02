#include "doctest/doctest/doctest.h"

#include "HedgehogUI/api/TextLayout.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace HUI;
using ContentLoader::LoadedFont;
using ContentLoader::LoadedGlyph;

namespace
{
    // A synthetic font with round numbers: ascent 8, descent -2, line gap 2 (a 12-pixel line),
    // every glyph 10 across with an 8x8 bitmap one pixel right of the pen, a 5-pixel space with no
    // bitmap, and "AV" kerned by -2. Glyph i sits at (10 * i, 0) in a 1024x16 atlas.
    LoadedFont MakeFont()
    {
        LoadedFont font;
        font.PixelHeight = 10.0f;
        font.Ascent      = 8.0f;
        font.Descent     = -2.0f;
        font.LineGap     = 2.0f;
        font.AtlasWidth  = 1024;
        font.AtlasHeight = 16;

        std::vector<uint32_t> codepoints = { ' ', '?' };
        for (uint32_t c = 'A'; c <= 'Z'; ++c)
            codepoints.push_back(c);
        for (uint32_t c = 'a'; c <= 'z'; ++c)
            codepoints.push_back(c);
        codepoints.push_back(0xE9); // é
        std::sort(codepoints.begin(), codepoints.end());

        for (const uint32_t codepoint : codepoints)
        {
            LoadedGlyph glyph;
            glyph.Codepoint = codepoint;
            glyph.AtlasX    = 10 * static_cast<uint32_t>(font.Glyphs.size());
            if (codepoint == ' ')
            {
                glyph.Advance = 5.0f;
            }
            else
            {
                glyph.Width   = 8;
                glyph.Height  = 8;
                glyph.OffsetX = 1.0f;
                glyph.OffsetY = -8.0f;
                glyph.Advance = 10.0f;
            }
            if (codepoint == '?')
                font.FallbackGlyph = static_cast<uint32_t>(font.Glyphs.size());
            font.Glyphs.push_back(glyph);
        }

        const auto index = [&](uint32_t codepoint) { return static_cast<uint32_t>(FindGlyph(font, codepoint) - font.Glyphs.data()); };
        font.Kerning.push_back({ index('A'), index('V'), -2.0f });
        return font;
    }

    const LoadedFont       FONT   = MakeFont();
    const HX::UiRect       SCREEN = { 0.0f, 0.0f, 800.0f, 600.0f };

    std::vector<std::string> LineTexts(std::string_view text, float maxWidth, bool wrap = true)
    {
        std::vector<TextLine> lines;
        BreakLines(FONT, text, maxWidth, wrap, lines);
        std::vector<std::string> texts;
        for (const TextLine& line : lines)
            texts.emplace_back(text.substr(line.Begin, line.End - line.Begin));
        return texts;
    }

    // The top-left corner of glyph quad i.
    std::pair<float, float> QuadAt(const HX::UiDrawList& list, size_t i)
    {
        REQUIRE(list.Vertices.size() >= (i + 1) * 4);
        return { list.Vertices[i * 4].Position[0], list.Vertices[i * 4].Position[1] };
    }

    using Corner = std::pair<float, float>;
}

TEST_CASE("Text - glyph lookup falls back for a codepoint the font lacks, and kerning is by glyph pair")
{
    CHECK(FindGlyph(FONT, 'a')->Codepoint == 'a');
    CHECK(FindGlyph(FONT, 0xE9)->Codepoint == 0xE9);
    CHECK(FindGlyph(FONT, '#')->Codepoint == '?');
    CHECK(FindGlyph(FONT, 0x20AC)->Codepoint == '?'); // €
    CHECK(FindGlyph(LoadedFont{}, 'a') == nullptr);

    const auto index = [](uint32_t codepoint) { return static_cast<uint32_t>(FindGlyph(FONT, codepoint) - FONT.Glyphs.data()); };
    CHECK(GetKerning(FONT, index('A'), index('V')) == -2.0f);
    CHECK(GetKerning(FONT, index('V'), index('A')) == 0.0f);
    CHECK(GetLineHeight(FONT) == 12.0f);
    CHECK(GetLineHeight(FONT, 1.5f) == 18.0f);
}

TEST_CASE("Text - lines break at newlines and, when wrapping, at word boundaries")
{
    using Lines = std::vector<std::string>;
    CHECK(LineTexts("hello world", 1000.0f) == Lines{ "hello world" });
    CHECK(LineTexts("hello world", 60.0f) == Lines{ "hello", "world" });
    CHECK(LineTexts("hello world", 60.0f, false) == Lines{ "hello world" });
    CHECK(LineTexts("hello   world", 60.0f) == Lines{ "hello", "world" }); // the spaces belong to no line
    CHECK(LineTexts("ab cd ef", 50.0f) == Lines{ "ab cd", "ef" });       // 20 + 5 + 20 fits in 50
    CHECK(LineTexts("ab\ncd", 1000.0f) == Lines{ "ab", "cd" });
    CHECK(LineTexts("ab\n\ncd", 1000.0f) == Lines{ "ab", "", "cd" });
    CHECK(LineTexts("ab\n", 1000.0f) == Lines{ "ab", "" });
    CHECK(LineTexts("ab\r\ncd", 1000.0f) == Lines{ "ab", "cd" });
    CHECK(LineTexts("", 1000.0f).empty());

    // A word wider than the line breaks between characters; a line always takes one.
    CHECK(LineTexts("abcdefgh", 35.0f) == Lines{ "abc", "def", "gh" });
    CHECK(LineTexts("abc", 0.0f) == Lines{ "a", "b", "c" });

    // Widths: advances and kerning, without the spaces that end a line.
    std::vector<TextLine> lines;
    BreakLines(FONT, "ab  \nAV x", 1000.0f, true, lines);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].Width == 20.0f);
    CHECK(lines[1].Width == 10.0f + -2.0f + 10.0f + 5.0f + 10.0f);
}

TEST_CASE("Text - lines align left, centre and right, and the block top, middle and bottom")
{
    const HX::UiRect rect = { 100.0f, 50.0f, 200.0f, 100.0f };
    const auto       firstGlyph = [&](TextAlign align, TextVerticalAlign vertical)
    {
        HX::UiDrawList list;
        TextStyle      style;
        style.Align         = align;
        style.VerticalAlign = vertical;
        LayoutText(list, FONT, "ab", rect, style, SCREEN);
        REQUIRE(list.Vertices.size() == 8);
        return QuadAt(list, 0);
    };

    // "ab" is 20 across; the bitmap is 1 right of the pen and its top at the ascent (8 above the baseline).
    CHECK(firstGlyph(TextAlign::Left, TextVerticalAlign::Top) == Corner{ 101.0f, 50.0f });
    CHECK(firstGlyph(TextAlign::Center, TextVerticalAlign::Top) == Corner{ 191.0f, 50.0f });
    CHECK(firstGlyph(TextAlign::Right, TextVerticalAlign::Top) == Corner{ 281.0f, 50.0f });

    // One line's block is ascent to descent, 10 high.
    CHECK(firstGlyph(TextAlign::Left, TextVerticalAlign::Middle) == Corner{ 101.0f, 95.0f });
    CHECK(firstGlyph(TextAlign::Left, TextVerticalAlign::Bottom) == Corner{ 101.0f, 140.0f });
}

TEST_CASE("Text - each line is aligned on its own and placed one line height below the last")
{
    const HX::UiRect rect = { 0.0f, 0.0f, 60.0f, 100.0f };
    HX::UiDrawList   list;
    TextStyle        style;
    style.Align = TextAlign::Right;
    LayoutText(list, FONT, "hello wor\nab", rect, style, SCREEN); // wraps to "hello", "wor", "ab"

    REQUIRE(list.Vertices.size() == 10 * 4);
    CHECK(QuadAt(list, 0) == Corner{ 11.0f, 0.0f });  // "hello", 50 across
    CHECK(QuadAt(list, 4) == Corner{ 51.0f, 0.0f });
    CHECK(QuadAt(list, 5) == Corner{ 31.0f, 12.0f }); // "wor", 30 across
    CHECK(QuadAt(list, 8) == Corner{ 41.0f, 24.0f }); // "ab", 20 across

    // Middle: three lines are 2 * 12 + 10 high.
    list.Clear();
    style.VerticalAlign = TextVerticalAlign::Middle;
    LayoutText(list, FONT, "hello wor\nab", rect, style, SCREEN);
    CHECK(QuadAt(list, 0).second == 33.0f);
}

TEST_CASE("Text - a quad samples its glyph's atlas rect, pens advance with kerning, and line origins snap to pixels")
{
    HX::UiDrawList   list;
    TextStyle        style;
    style.Texture    = 4;
    style.Color      = 0x80FF00FFu;
    const HX::UiRect rect = { 0.5f, 0.25f, 100.0f, 20.0f };
    LayoutText(list, FONT, "AV", rect, style, SCREEN);

    REQUIRE(list.Commands.size() == 1);
    CHECK(list.Commands[0].Texture == 4);
    CHECK(QuadAt(list, 0) == Corner{ 2.0f, 0.0f });  // the line's origin (0.5, 8.25) snaps to (1, 8)
    CHECK(QuadAt(list, 1) == Corner{ 10.0f, 0.0f }); // A's advance, less the AV kerning

    const LoadedGlyph& a = *FindGlyph(FONT, 'A');
    const HX::UiVertex& topLeft     = list.Vertices[0];
    const HX::UiVertex& bottomRight = list.Vertices[2];
    CHECK(topLeft.Uv[0] == doctest::Approx(static_cast<float>(a.AtlasX) / 1024.0f));
    CHECK(topLeft.Uv[1] == 0.0f);
    CHECK(bottomRight.Uv[0] == doctest::Approx(static_cast<float>(a.AtlasX + 8) / 1024.0f));
    CHECK(bottomRight.Uv[1] == doctest::Approx(0.5f));
    CHECK(topLeft.Color == 0x80FF00FFu);
}

TEST_CASE("Text - a missing glyph or malformed UTF-8 draws the fallback glyph")
{
    const LoadedGlyph& question = *FindGlyph(FONT, '?');
    const LoadedGlyph& e        = *FindGlyph(FONT, 0xE9);
    const auto         uvOf     = [](const HX::UiDrawList& list, size_t i) { return list.Vertices[i * 4].Uv[0]; };

    HX::UiDrawList list;
    LayoutText(list, FONT, "a\xE2\x82\xAC" "b\xFF" "c\xC3\xA9", SCREEN, TextStyle{}, SCREEN); // a € b <bad> c é
    REQUIRE(list.Vertices.size() == 6 * 4);
    CHECK(uvOf(list, 1) == doctest::Approx(static_cast<float>(question.AtlasX) / 1024.0f));
    CHECK(uvOf(list, 3) == doctest::Approx(static_cast<float>(question.AtlasX) / 1024.0f));
    CHECK(uvOf(list, 5) == doctest::Approx(static_cast<float>(e.AtlasX) / 1024.0f));

    // Truncated sequences at the end, and a font with no glyphs at all, never read past the text.
    list.Clear();
    LayoutText(list, FONT, "a\xE2\x82", SCREEN, TextStyle{}, SCREEN);
    CHECK(list.Vertices.size() == 3 * 4);
    list.Clear();
    LayoutText(list, LoadedFont{}, "abc", SCREEN, TextStyle{}, SCREEN);
    CHECK(list.Vertices.empty());
}

TEST_CASE("Text - overflowing text is clipped to its rect inside the scissor")
{
    HX::UiDrawList   list;
    TextStyle        style;
    style.Wrap            = false;
    const HX::UiRect rect = { 0.0f, 0.0f, 35.0f, 20.0f };
    LayoutText(list, FONT, "abcdefghij", rect, style, SCREEN);

    // Glyph quads start at 1, 11, 21 and 31; the one at 41 lies wholly outside and is dropped.
    CHECK(list.Vertices.size() == 4 * 4);
    REQUIRE(list.Commands.size() == 1);
    CHECK(list.Commands[0].Scissor == rect);

    // The clip is the rect cut by the scissor; a rect outside the scissor adds nothing.
    list.Clear();
    LayoutText(list, FONT, "abc", rect, style, { 10.0f, 0.0f, 800.0f, 5.0f });
    REQUIRE(list.Commands.size() == 1);
    CHECK(list.Commands[0].Scissor == HX::UiRect{ 10.0f, 0.0f, 25.0f, 5.0f });
    list.Clear();
    LayoutText(list, FONT, "abc", rect, style, { 100.0f, 100.0f, 50.0f, 50.0f });
    CHECK(list.Commands.empty());
}

TEST_CASE("Text - laying text out again into a cleared list keeps its storage")
{
    HX::UiDrawList list;
    LayoutText(list, FONT, "hello world, wrapped\nand aligned", { 0.0f, 0.0f, 80.0f, 100.0f }, TextStyle{}, SCREEN);
    const size_t vertices = list.Vertices.size();
    const auto*  storage  = list.Vertices.data();
    for (int i = 0; i < 5; ++i)
    {
        list.Clear();
        LayoutText(list, FONT, "hello world, wrapped\nand aligned", { 0.0f, 0.0f, 80.0f, 100.0f }, TextStyle{}, SCREEN);
    }
    CHECK(list.Vertices.size() == vertices);
    CHECK(list.Vertices.data() == storage);
}
