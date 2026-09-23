#include "HedgehogRenderer/Graph/RGTypes.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

TEST_CASE("Absolute resolves to its fixed width/height, ignoring the reference size")
{
    const RGSizePolicy policy = RGSizePolicy::MakeAbsolute(512, 256);

    uint32_t width = 0, height = 0;
    policy.Resolve(/*reference*/ 1920, 1080, width, height);

    CHECK(width == 512);
    CHECK(height == 256);
}

TEST_CASE("RelativeToResult scales the reference (the view's result size) by its factor")
{
    const RGSizePolicy half = RGSizePolicy::MakeRelativeToResult(0.5f);

    uint32_t width = 0, height = 0;
    half.Resolve(/*result*/ 1920, 1080, width, height);

    CHECK(width == 960);
    CHECK(height == 540);
}

TEST_CASE("RelativeToSwapchain scales the reference (the swapchain size) by its factor")
{
    const RGSizePolicy full = RGSizePolicy::MakeRelativeToSwapchain(1.0f);

    uint32_t width = 0, height = 0;
    full.Resolve(/*swapchain*/ 1280, 720, width, height);

    CHECK(width == 1280);
    CHECK(height == 720);
}

TEST_CASE("A quarter-resolution policy (e.g. AO) rounds down like a plain float-to-int cast")
{
    const RGSizePolicy quarter = RGSizePolicy::MakeRelativeToResult(0.25f);

    uint32_t width = 0, height = 0;
    quarter.Resolve(1921, 1081, width, height);

    CHECK(width == 480);
    CHECK(height == 270);
}

TEST_CASE("Resolve needs no device, window or swapchain object — a plain reference size is enough")
{
    // The whole point of RGSizePolicy::Resolve is that it's a pure function (RENDERING.md
    // section 5.3: "compilation is device-free"). This test is the assertion that nothing here
    // requires more than two integers in, two integers out.
    const RGSizePolicy policy = RGSizePolicy::MakeRelativeToSwapchain(2.0f);
    uint32_t width = 0, height = 0;
    policy.Resolve(100, 50, width, height);
    CHECK(width == 200);
    CHECK(height == 100);
}
