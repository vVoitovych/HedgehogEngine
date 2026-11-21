#include "ColorLut.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
    std::vector<uint8_t> MakeNeutralColorLut()
    {
        std::vector<uint8_t> pixels(static_cast<size_t>(COLOR_LUT_WIDTH) * COLOR_LUT_HEIGHT * 4);
        const auto           channel = [](uint32_t step)
        { return static_cast<uint8_t>(std::lround(255.0f * static_cast<float>(step) / (COLOR_LUT_SIZE - 1))); };
        for (uint32_t y = 0; y < COLOR_LUT_HEIGHT; ++y)
        {
            for (uint32_t x = 0; x < COLOR_LUT_WIDTH; ++x)
            {
                uint8_t* texel = &pixels[(static_cast<size_t>(y) * COLOR_LUT_WIDTH + x) * 4];
                texel[0]       = channel(x % COLOR_LUT_SIZE);
                texel[1]       = channel(y);
                texel[2]       = channel(x / COLOR_LUT_SIZE);
                texel[3]       = 255;
            }
        }
        return pixels;
    }

    bool IsColorLutSize(uint32_t width, uint32_t height)
    {
        return width == COLOR_LUT_WIDTH && height == COLOR_LUT_HEIGHT;
    }

    std::array<uint32_t, 2> GetColorLutTexel(float r, float g, float b)
    {
        const float last  = static_cast<float>(COLOR_LUT_SIZE - 1);
        const auto  index = [last](float value)
        { return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * last)); };
        return { index(b) * COLOR_LUT_SIZE + index(r), index(g) };
    }
}
