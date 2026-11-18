#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/HedgehogWindow/api/CursorMode.hpp"

#include <initializer_list>

using namespace HW;

static_assert(ResolveCursorMode(CursorMode::Locked, true) == CursorMode::Locked);

TEST_CASE("Cursor mode - the requested mode shows only while the window has focus")
{
    for (const CursorMode mode : { CursorMode::Normal, CursorMode::Hidden, CursorMode::Locked, CursorMode::Confined })
    {
        CHECK(ResolveCursorMode(mode, true) == mode);
        CHECK(ResolveCursorMode(mode, false) == CursorMode::Normal);
    }
}
