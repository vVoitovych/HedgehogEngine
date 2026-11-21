#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace Renderer
{
    // A colour grading LUT: a strip of COLOR_LUT_SIZE tiles, each COLOR_LUT_SIZE x COLOR_LUT_SIZE,
    // side by side (1024 x 32). Tile b holds blue b / 31; within it red runs along x and green along
    // y, all in display-encoded (sRGB) values.
    inline constexpr uint32_t COLOR_LUT_SIZE   = 32;
    inline constexpr uint32_t COLOR_LUT_WIDTH  = COLOR_LUT_SIZE * COLOR_LUT_SIZE;
    inline constexpr uint32_t COLOR_LUT_HEIGHT = COLOR_LUT_SIZE;

    // The LUT that changes nothing, RGBA8, row-major from the top.
    [[nodiscard]] std::vector<uint8_t> MakeNeutralColorLut();

    [[nodiscard]] bool IsColorLutSize(uint32_t width, uint32_t height);

    // The texel ToneMap's lookup reads (its nearer blue slice) for an encoded colour, each channel in
    // 0 to 1: x and y as Aces.frag computes them, at texel centres.
    [[nodiscard]] std::array<uint32_t, 2> GetColorLutTexel(float r, float g, float b);
}
