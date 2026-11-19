#include "HedgehogEngine/HedgehogWindow/api/DisplayMode.hpp"

#include "doctest/doctest/doctest.h"

#include <vector>

using namespace HW;

TEST_CASE("DisplayMode - normalizing keeps one mode per size, the fastest, smallest first")
{
    const std::vector<VideoMode> modes = NormalizeVideoModes({ { 1920, 1080, 60 },
                                                               { 1280, 720, 60 },
                                                               { 1920, 1080, 144 },
                                                               { 0, 0, 60 },
                                                               { -1, 720, 60 },
                                                               { 1920, 1080, 120 } });
    REQUIRE(modes.size() == 2);
    CHECK(modes[0] == VideoMode{ 1280, 720, 60 });
    CHECK(modes[1] == VideoMode{ 1920, 1080, 144 });

    CHECK(NormalizeVideoModes({}).empty());
}

TEST_CASE("DisplayMode - choosing a video mode")
{
    const std::vector<VideoMode> modes = { { 1280, 720, 60 }, { 1600, 900, 60 },
                                           { 1920, 1080, 144 }, { 2560, 1440, 60 } };

    CHECK(ChooseVideoMode(modes, 1920, 1080) == VideoMode{ 1920, 1080, 144 });
    CHECK(ChooseVideoMode(modes, 1700, 950) == VideoMode{ 1920, 1080, 144 });
    // 1600x900 is wide enough but not tall enough.
    CHECK(ChooseVideoMode(modes, 1600, 1000) == VideoMode{ 1920, 1080, 144 });
    CHECK(ChooseVideoMode(modes, 4000, 3000) == VideoMode{ 2560, 1440, 60 });
    CHECK(ChooseVideoMode(modes, 0, 0) == VideoMode{ 2560, 1440, 60 });
    CHECK(ChooseVideoMode(modes, 0, 720) == VideoMode{ 2560, 1440, 60 });
    CHECK(ChooseVideoMode(modes, -1, -1) == VideoMode{ 2560, 1440, 60 });
    CHECK(ChooseVideoMode(modes, 800, 600) == VideoMode{ 1280, 720, 60 });
    CHECK_FALSE(ChooseVideoMode({}, 800, 600).has_value());
}
