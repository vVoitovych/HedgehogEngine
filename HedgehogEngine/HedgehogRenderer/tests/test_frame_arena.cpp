#include "HedgehogRenderer/Graph/FrameArena.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    struct Small { int Value = 0; };
    struct Aligned16 { alignas(16) double Value = 0.0; };
}

TEST_CASE("Create returns a usable, correctly constructed object")
{
    FrameArena arena(1024);
    Small* s = arena.Create<Small>(42);
    REQUIRE(s != nullptr);
    CHECK(s->Value == 42);
}

TEST_CASE("Consecutive Create calls return distinct, non-overlapping addresses")
{
    FrameArena arena(1024);
    Small* a = arena.Create<Small>(1);
    Small* b = arena.Create<Small>(2);
    CHECK(a != b);
    CHECK(a->Value == 1);
    CHECK(b->Value == 2); // writing b must not have clobbered a
}

TEST_CASE("Create respects alignment requirements")
{
    FrameArena arena(1024);
    (void)arena.Create<Small>(); // offset the bump pointer off a 16-byte boundary
    Aligned16* aligned = arena.Create<Aligned16>();
    CHECK(reinterpret_cast<uintptr_t>(aligned) % alignof(Aligned16) == 0);
}

TEST_CASE("Reset rewinds usage so the arena can be fully reused without growing")
{
    FrameArena arena(1024);
    arena.Create<Small>();
    arena.Create<Small>();
    CHECK(arena.UsedBytes() > 0);

    arena.Reset();
    CHECK(arena.UsedBytes() == 0);

    // Reusing the same capacity after Reset must not exhaust the arena.
    for (int i = 0; i < 10; ++i)
        arena.Create<Small>(i);
    CHECK(arena.UsedBytes() <= arena.CapacityBytes());
}
