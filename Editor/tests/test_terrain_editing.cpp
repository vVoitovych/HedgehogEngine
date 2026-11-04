#include "doctest/doctest/doctest.h"

#include "Tools/Terrain/TerrainEditing.hpp"

#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include <limits>

using namespace Editor;
using HedgehogEngine::TerrainData;

namespace
{
    // A 32 m tile of 33 x 33 samples on the plane 1 + 0.25 x - 0.125 y, with a bump at (16, 16).
    TerrainData MakeSlope()
    {
        TerrainData data = HedgehogEngine::MakeDefaultTerrain();
        data.Size        = 32.0f;
        data.Resolution  = 33;
        data.Heights.resize(33 * 33);
        for (uint32_t row = 0; row < 33; ++row)
            for (uint32_t column = 0; column < 33; ++column)
                data.Heights[row * 33 + column] = 10.0f + 0.25f * column - 0.125f * row;
        return data;
    }
}

TEST_CASE("Terrain editing - every offered resolution is a valid one")
{
    for (const uint32_t resolution : TERRAIN_RESOLUTIONS)
        CHECK(HedgehogEngine::IsValidTerrainResolution(resolution));
}

TEST_CASE("Terrain editing - resampling keeps the surface's shape at any resolution")
{
    TerrainData data = MakeSlope();
    REQUIRE(ResampleTerrain(data, 65));
    CHECK(data.Resolution == 65);
    REQUIRE(data.Heights.size() == 65 * 65);
    // Every new sample on the old plane, half a metre apart now.
    for (uint32_t row = 0; row < 65; row += 7)
        for (uint32_t column = 0; column < 65; column += 5)
            CHECK(data.Heights[row * 65 + column] == doctest::Approx(10.0f + 0.25f * column * 0.5f - 0.125f * row * 0.5f));

    // Down again: the corners and the sampled points the coarse grid shares.
    REQUIRE(ResampleTerrain(data, 33));
    const TerrainData original = MakeSlope();
    for (size_t i = 0; i < data.Heights.size(); ++i)
        CHECK(data.Heights[i] == doctest::Approx(original.Heights[i]));
    CHECK(HedgehogEngine::SampleTerrainHeight(data, 7.3f, 21.9f) ==
          doctest::Approx(HedgehogEngine::SampleTerrainHeight(original, 7.3f, 21.9f)));
}

TEST_CASE("Terrain editing - an invalid resolution changes nothing")
{
    TerrainData data = MakeSlope();
    CHECK_FALSE(ResampleTerrain(data, 64));
    CHECK_FALSE(ResampleTerrain(data, 2049));
    CHECK(data == MakeSlope());
    CHECK(ResampleTerrain(data, 33)); // the same resolution keeps it as it is
    CHECK(data == MakeSlope());
}

TEST_CASE("Terrain editing - a new height scale keeps heights that fit and clamps the rest")
{
    TerrainData data = MakeSlope();
    REQUIRE(SetTerrainHeightScale(data, 15.0f));
    CHECK(data.HeightScale == 15.0f);
    CHECK(data.Heights[0] == 10.0f);
    CHECK(data.Heights[32] == 15.0f); // 18 clamped
    CHECK_FALSE(SetTerrainHeightScale(data, 0.0f));
    CHECK_FALSE(SetTerrainHeightScale(data, std::numeric_limits<float>::infinity()));
    CHECK(data.HeightScale == 15.0f);
}
