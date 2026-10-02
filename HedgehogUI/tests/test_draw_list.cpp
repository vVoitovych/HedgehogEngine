#include "doctest/doctest/doctest.h"

#include "HedgehogUI/api/UiDraw.hpp"

#include <atomic>
#include <cstdlib>
#include <new>
#include <vector>

using namespace HUI;

// Counts global allocations while s_Counting is set, so a test can prove a span of code allocates
// nothing. This test executable owns the global operator new.
namespace
{
    std::atomic<bool>   s_Counting    = false;
    std::atomic<size_t> s_Allocations = 0;
}

void* operator new(size_t size)
{
    if (s_Counting)
        ++s_Allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
    std::free(memory);
}

namespace
{
    const HX::UiRect SCREEN = { 0.0f, 0.0f, 800.0f, 600.0f };

    UiQuad Quad(float x, float y, uint32_t texture = HX::UI_NO_TEXTURE)
    {
        UiQuad quad;
        quad.Rect    = { x, y, 10.0f, 20.0f };
        quad.Texture = texture;
        return quad;
    }
}

TEST_CASE("Draw list - a quad is four vertices, two triangles and one command")
{
    HX::UiDrawList list;
    UiQuad         quad = Quad(10.0f, 30.0f, 7);
    quad.Color          = PackColor(1.0f, 0.0f, 0.0f);
    quad.Uv             = { 0.25f, 0.5f, 0.5f, 0.25f };
    AppendQuad(list, quad, SCREEN);

    REQUIRE(list.Vertices.size() == 4);
    const HX::UiVertex& topLeft     = list.Vertices[0];
    const HX::UiVertex& bottomRight = list.Vertices[2];
    CHECK(topLeft.Position[0] == 10.0f);
    CHECK(topLeft.Position[1] == 30.0f);
    CHECK(bottomRight.Position[0] == 20.0f);
    CHECK(bottomRight.Position[1] == 50.0f);
    CHECK(topLeft.Uv[0] == 0.25f);
    CHECK(topLeft.Uv[1] == 0.5f);
    CHECK(bottomRight.Uv[0] == 0.75f);
    CHECK(bottomRight.Uv[1] == 0.75f);
    CHECK(topLeft.Color == 0xFF0000FFu);

    CHECK(list.Indices == std::vector<uint16_t>{ 0, 1, 2, 0, 2, 3 });
    REQUIRE(list.Commands.size() == 1);
    CHECK(list.Commands[0].Texture == 7);
    CHECK(list.Commands[0].Scissor == SCREEN);
    CHECK(list.Commands[0].FirstIndex == 0);
    CHECK(list.Commands[0].IndexCount == 6);
}

TEST_CASE("Draw list - consecutive quads with one texture and scissor batch; a change starts a new command")
{
    HX::UiDrawList   list;
    const HX::UiRect panel = { 100.0f, 100.0f, 200.0f, 200.0f };
    AppendQuad(list, Quad(0.0f, 0.0f, 1), SCREEN);
    AppendQuad(list, Quad(20.0f, 0.0f, 1), SCREEN);   // joins
    AppendQuad(list, Quad(40.0f, 0.0f, 2), SCREEN);   // another texture
    AppendQuad(list, Quad(150.0f, 150.0f, 2), panel); // same texture, another scissor
    AppendQuad(list, Quad(60.0f, 0.0f, 1), SCREEN);   // texture 1 again, but not consecutive
    AppendQuad(list, Quad(80.0f, 0.0f), SCREEN);      // solid
    AppendQuad(list, Quad(90.0f, 0.0f), SCREEN);      // solid, joins

    REQUIRE(list.Commands.size() == 5);
    const auto check = [&](size_t i, uint32_t texture, const HX::UiRect& scissor, uint32_t firstIndex, uint32_t count)
    {
        CAPTURE(i);
        CHECK(list.Commands[i].Texture == texture);
        CHECK(list.Commands[i].Scissor == scissor);
        CHECK(list.Commands[i].VertexOffset == firstIndex / 6 * 4); // each command indexes from its first vertex
        CHECK(list.Commands[i].FirstIndex == firstIndex);
        CHECK(list.Commands[i].IndexCount == count);
    };
    check(0, 1, SCREEN, 0, 12);
    check(1, 2, SCREEN, 12, 6);
    check(2, 2, panel, 18, 6);
    check(3, 1, SCREEN, 24, 6);
    check(4, HX::UI_NO_TEXTURE, SCREEN, 30, 12);

    // The second quad of the first command indexes its own four vertices.
    CHECK(std::vector<uint16_t>(list.Indices.begin() + 6, list.Indices.begin() + 12) ==
          std::vector<uint16_t>{ 4, 5, 6, 4, 6, 7 });
    CHECK(list.Vertices.size() == 28);
}

TEST_CASE("Draw list - quads with no area or outside the scissor add nothing")
{
    HX::UiDrawList list;
    UiQuad         empty = Quad(10.0f, 10.0f);
    empty.Rect.Width     = 0.0f;
    AppendQuad(list, empty, SCREEN);
    UiQuad inverted      = Quad(10.0f, 10.0f);
    inverted.Rect.Height = -5.0f;
    AppendQuad(list, inverted, SCREEN);
    AppendQuad(list, Quad(900.0f, 10.0f), SCREEN);
    AppendQuad(list, Quad(-10.0f, 10.0f), SCREEN); // ends exactly at the scissor's left edge
    CHECK(list.Vertices.empty());
    CHECK(list.Indices.empty());
    CHECK(list.Commands.empty());

    AppendQuad(list, Quad(-5.0f, 10.0f), SCREEN); // partly inside: kept, the scissor clips it
    CHECK(list.Commands.size() == 1);
}

TEST_CASE("Draw list - a command never indexes past 16 bits; the next quad starts a new vertex range")
{
    HX::UiDrawList list;
    constexpr int  QUADS_PER_COMMAND = 65536 / 4;
    for (int i = 0; i < QUADS_PER_COMMAND + 1; ++i)
        AppendQuad(list, Quad(1.0f, 1.0f, 3), SCREEN);

    REQUIRE(list.Commands.size() == 2);
    CHECK(list.Commands[0].IndexCount == QUADS_PER_COMMAND * 6u);
    CHECK(list.Commands[1].VertexOffset == 65536u);
    CHECK(list.Commands[1].FirstIndex == QUADS_PER_COMMAND * 6u);
    CHECK(list.Commands[1].IndexCount == 6);
    CHECK(list.Indices[list.Commands[1].FirstIndex] == 0); // relative to its VertexOffset
}

TEST_CASE("Draw list - rebuilding into a cleared list allocates nothing")
{
    const HX::UiRect panel = { 0.0f, 0.0f, 400.0f, 300.0f };
    const auto build = [&](HX::UiDrawList& list)
    {
        for (int i = 0; i < 200; ++i)
            AppendQuad(list, Quad(static_cast<float>(i % 40) * 10.0f, 10.0f, static_cast<uint32_t>(i % 3)),
                       i % 5 == 0 ? panel : SCREEN);
    };

    HX::UiDrawList list;
    build(list);
    const size_t vertices = list.Vertices.size();
    const size_t commands = list.Commands.size();

    // Prove the counter counts, then rebuild the same UI with it on.
    s_Allocations = 0;
    s_Counting    = true;
    std::vector<int>* probe = new std::vector<int>(1);
    s_Counting    = false;
    delete probe;
    REQUIRE(s_Allocations > 0);

    s_Allocations = 0;
    s_Counting    = true;
    for (int frame = 0; frame < 10; ++frame)
    {
        list.Clear();
        build(list);
    }
    s_Counting = false;
    CHECK(s_Allocations == 0);
    CHECK(list.Vertices.size() == vertices);
    CHECK(list.Commands.size() == commands);
}

TEST_CASE("Draw list - PackColor packs RGBA8 with red lowest, clamped and rounded")
{
    CHECK(PackColor(1.0f, 0.0f, 0.0f) == 0xFF0000FFu);
    CHECK(PackColor(0.0f, 0.5f, 1.0f, 0.0f) == 0x00FF8000u);
    CHECK(PackColor(2.0f, -1.0f, 0.0f, 1.0f) == 0xFF0000FFu);
    CHECK(UiQuad{}.Color == PackColor(1.0f, 1.0f, 1.0f, 1.0f));
}
