#include "doctest/doctest/doctest.h"

#include "Tools/Terrain/TerrainBrush.hpp"

#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <cmath>

using namespace Editor;
using HedgehogEngine::TerrainData;

namespace
{
    // A flat 32 m tile of 33 x 33 samples (1 m apart) at height h.
    TerrainData MakeFlat(float h)
    {
        TerrainData data = HedgehogEngine::MakeDefaultTerrain();
        data.Size        = 32.0f;
        data.Resolution  = 33;
        data.Heights.assign(33 * 33, h);
        return data;
    }

    float& At(TerrainData& data, int column, int row) { return data.Heights[row * 33 + column]; }

    TerrainBrush Brush(TerrainBrushTool tool, float radius = 4.0f, float strength = 2.0f, float falloff = 0.5f)
    {
        TerrainBrush brush;
        brush.Tool     = tool;
        brush.Radius   = radius;
        brush.Strength = strength;
        brush.Falloff  = falloff;
        return brush;
    }
}

TEST_CASE("Terrain brush - the weight is full inside, eases out and is nothing at the radius")
{
    CHECK(GetBrushWeight(0.0f, 4.0f, 0.5f) == 1.0f);
    CHECK(GetBrushWeight(2.0f, 4.0f, 0.5f) == 1.0f);
    CHECK(GetBrushWeight(3.0f, 4.0f, 0.5f) == doctest::Approx(0.5f));
    CHECK(GetBrushWeight(3.5f, 4.0f, 0.5f) < 0.5f);
    CHECK(GetBrushWeight(4.0f, 4.0f, 0.5f) == 0.0f);
    CHECK(GetBrushWeight(9.0f, 4.0f, 0.5f) == 0.0f);
    CHECK(GetBrushWeight(3.9f, 4.0f, 0.0f) == 1.0f); // no falloff: a hard edge
    CHECK(GetBrushWeight(0.0f, 0.0f, 0.5f) == 0.0f);
}

TEST_CASE("Terrain brush - raising moves only the samples inside the radius, the centre the most")
{
    TerrainData data = MakeFlat(10.0f);
    REQUIRE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Raise), 16.0f, 16.0f, 0.5f));
    CHECK(At(data, 16, 16) == doctest::Approx(11.0f)); // 2 m/s for half a second
    CHECK(At(data, 18, 16) == doctest::Approx(11.0f)); // still in the full part
    CHECK(At(data, 19, 16) > 10.0f);
    CHECK(At(data, 19, 16) < 11.0f);
    CHECK(At(data, 20, 16) == 10.0f); // at the radius
    CHECK(At(data, 16, 21) == 10.0f);
    CHECK(At(data, 0, 0) == 10.0f);

    // Lowering takes it back down, and never below 0.
    REQUIRE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Lower), 16.0f, 16.0f, 0.5f));
    CHECK(At(data, 16, 16) == doctest::Approx(10.0f));
    REQUIRE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Lower, 4.0f, 100.0f), 16.0f, 16.0f, 1.0f));
    CHECK(At(data, 16, 16) == 0.0f);
    CHECK(At(data, 20, 16) == 10.0f);
}

TEST_CASE("Terrain brush - raising stops at the height scale and a brush by the edge stays on the tile")
{
    TerrainData data = MakeFlat(60.0f);
    REQUIRE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Raise, 4.0f, 100.0f), 0.0f, 0.0f, 1.0f));
    CHECK(At(data, 0, 0) == data.HeightScale);
    CHECK(At(data, 2, 0) == data.HeightScale);
    CHECK(At(data, 32, 32) == 60.0f);
    // Nothing to change: a stroke off the tile, a zero time and a zero radius.
    CHECK_FALSE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Raise), 100.0f, 100.0f, 1.0f));
    CHECK_FALSE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Raise), 16.0f, 16.0f, 0.0f));
    CHECK_FALSE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Raise, 0.0f), 16.0f, 16.0f, 1.0f));
}

TEST_CASE("Terrain brush - smoothing pulls a spike towards its neighbours' average")
{
    TerrainData data  = MakeFlat(10.0f);
    At(data, 16, 16)  = 19.0f;
    REQUIRE(ApplyTerrainBrush(data, Brush(TerrainBrushTool::Smooth, 3.0f, 100.0f), 16.0f, 16.0f, 1.0f));
    // The spike becomes the average of its 3 x 3: (19 + 8 * 10) / 9 = 11; its neighbours rise from it.
    CHECK(At(data, 16, 16) == doctest::Approx(11.0f));
    CHECK(At(data, 17, 16) == doctest::Approx(11.0f));
    CHECK(At(data, 18, 16) == doctest::Approx(10.0f));
    // A flat tile stays flat.
    TerrainData flat = MakeFlat(5.0f);
    CHECK_FALSE(ApplyTerrainBrush(flat, Brush(TerrainBrushTool::Smooth), 16.0f, 16.0f, 1.0f));
}

TEST_CASE("Terrain brush - flattening converges to the height the stroke began at")
{
    TerrainData data = MakeFlat(10.0f);
    for (int row = 0; row < 33; ++row)
        for (int column = 0; column < 33; ++column)
            At(data, column, row) = 10.0f + 0.5f * column;
    TerrainBrush brush  = Brush(TerrainBrushTool::Flatten, 5.0f, 2.0f);
    brush.FlattenHeight = 18.0f;
    for (int frame = 0; frame < 120; ++frame)
        ApplyTerrainBrush(data, brush, 16.0f, 16.0f, 1.0f / 60.0f);
    CHECK(At(data, 16, 16) == doctest::Approx(18.0f).epsilon(0.01));
    CHECK(At(data, 14, 16) == doctest::Approx(18.0f).epsilon(0.01));
    CHECK(At(data, 22, 16) == doctest::Approx(10.0f + 0.5f * 22)); // outside the radius
}

TEST_CASE("Terrain brush - the ring lies on the surface at the radius")
{
    TerrainData data = MakeFlat(0.0f);
    for (int row = 0; row < 33; ++row)
        for (int column = 0; column < 33; ++column)
            At(data, column, row) = 0.25f * column;
    const std::vector<HM::Vector3> ring = MakeBrushRing(data, 16.0f, 12.0f, 3.0f, 32);
    REQUIRE(ring.size() == 32);
    for (const HM::Vector3& point : ring)
    {
        CHECK(std::hypot(point.x() - 16.0f, point.y() - 12.0f) == doctest::Approx(3.0f));
        CHECK(point.z() == doctest::Approx(0.25f * point.x()));
    }
}
