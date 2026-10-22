#include "HedgehogEngine/HedgehogWindow/api/TitleBar.hpp"

#include "doctest/doctest/doctest.h"

#include <array>

using namespace HW;

TEST_CASE("TitleBar - the top edge resizes, caption areas drag, the rest is the application's")
{
    // Two gaps in a 36-pixel bar: the logo and title, and the space before the window buttons.
    constexpr std::array<ClientRect, 2> captions = { ClientRect{ 0, 0, 200, 36 }, ClientRect{ 600, 0, 400, 36 } };

    CHECK(HitTestTitleBar(100, 2, false, 6, captions) == TitleBarHit::Top);
    CHECK(HitTestTitleBar(100, 6, false, 6, captions) == TitleBarHit::Caption);
    CHECK(HitTestTitleBar(800, 20, false, 6, captions) == TitleBarHit::Caption);
    // A menu between the gaps, a window button after the last one, and below the bar.
    CHECK(HitTestTitleBar(300, 20, false, 6, captions) == TitleBarHit::Client);
    CHECK(HitTestTitleBar(1000, 20, false, 6, captions) == TitleBarHit::Client);
    CHECK(HitTestTitleBar(100, 36, false, 6, captions) == TitleBarHit::Client);
    // A maximized window has no edge to resize from.
    CHECK(HitTestTitleBar(100, 2, true, 6, captions) == TitleBarHit::Caption);
    CHECK(HitTestTitleBar(300, 2, true, 6, {}) == TitleBarHit::Client);
}
