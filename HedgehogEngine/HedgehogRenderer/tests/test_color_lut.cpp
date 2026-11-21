#include "../src/Graph/ColorLut.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace Renderer;

TEST_CASE("Colour LUT - the neutral strip maps every texel centre to itself")
{
    const std::vector<uint8_t> lut = MakeNeutralColorLut();
    REQUIRE(lut.size() == static_cast<size_t>(COLOR_LUT_WIDTH) * COLOR_LUT_HEIGHT * 4);
    const auto value = [](uint32_t step) { return static_cast<int>(std::lround(255.0f * step / (COLOR_LUT_SIZE - 1))); };
    for (uint32_t b = 0; b < COLOR_LUT_SIZE; ++b)
    {
        for (uint32_t g = 0; g < COLOR_LUT_SIZE; ++g)
        {
            for (uint32_t r = 0; r < COLOR_LUT_SIZE; ++r)
            {
                // The colour of a texel centre, as the shader computes its texel, reads back as itself.
                const float last  = static_cast<float>(COLOR_LUT_SIZE - 1);
                const auto  texel = GetColorLutTexel(r / last, g / last, b / last);
                const uint8_t* rgba = &lut[(static_cast<size_t>(texel[1]) * COLOR_LUT_WIDTH + texel[0]) * 4];
                if (rgba[0] != value(r) || rgba[1] != value(g) || rgba[2] != value(b) || rgba[3] != 255)
                {
                    CAPTURE(r);
                    CAPTURE(g);
                    CAPTURE(b);
                    FAIL("a texel centre does not map to itself");
                }
            }
        }
    }
}

TEST_CASE("Colour LUT - its size, and the texel a colour reads, clamped to the strip")
{
    CHECK(IsColorLutSize(1024, 32));
    CHECK_FALSE(IsColorLutSize(1, 1)); // the registry's magenta stand-in for a missing file
    CHECK_FALSE(IsColorLutSize(512, 32));
    CHECK_FALSE(IsColorLutSize(1024, 16));
    CHECK(GetColorLutTexel(0.0f, 0.0f, 0.0f) == std::array<uint32_t, 2>{ 0, 0 });
    CHECK(GetColorLutTexel(1.0f, 1.0f, 1.0f) == std::array<uint32_t, 2>{ 1023, 31 });
    CHECK(GetColorLutTexel(2.0f, -1.0f, 0.0f) == std::array<uint32_t, 2>{ 31, 0 });
    CHECK(GetColorLutTexel(0.0f, 0.0f, 1.0f / 31.0f) == std::array<uint32_t, 2>{ 32, 0 });
}
