#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include <cmath>
#include <string>

using namespace HedgehogEngine;

namespace
{
    // A small tile with varied heights, splat and layers.
    TerrainData MakeVariedTerrain()
    {
        TerrainData data     = MakeDefaultTerrain();
        data.Size            = 64.5f;
        data.Resolution      = 33;
        data.HeightScale     = 20.0f;
        data.SplatResolution = 16;
        data.Heights.resize(33 * 33);
        for (size_t i = 0; i < data.Heights.size(); ++i)
            data.Heights[i] = 10.0f + 9.5f * std::sin(static_cast<float>(i) * 0.37f);
        data.Splat.resize(16 * 16 * 4);
        for (size_t i = 0; i < data.Splat.size(); ++i)
            data.Splat[i] = static_cast<uint8_t>(i * 7);
        data.Layers[0] = { "Textures/Grass.png", "Textures/Grass_Normal.png", 12.0f, 0.0f, 0.9f };
        data.Layers[2] = { "Textures/Rock.png", "", 4.0f, 0.25f, 0.6f };
        return data;
    }

    // text with the line starting with key replaced by replacement (removed when empty).
    std::string ReplaceLine(const std::string& text, const std::string& key, const std::string& replacement)
    {
        const size_t start = text.find("\n" + key) + 1;
        const size_t end   = text.find('\n', start);
        return text.substr(0, start) + replacement + (replacement.empty() ? "" : "\n") + text.substr(end + 1);
    }
}

TEST_CASE("The default terrain is a flat 128 m tile of 129 x 129 samples painted with layer 0")
{
    const TerrainData data = MakeDefaultTerrain();
    CHECK(data.Size == 128.0f);
    CHECK(data.Resolution == 129);
    CHECK(data.SplatResolution == 256);
    REQUIRE(data.Heights.size() == 129 * 129);
    CHECK(data.Heights.front() == 0.0f);
    CHECK(data.Heights.back() == 0.0f);
    REQUIRE(data.Splat.size() == 256 * 256 * 4);
    CHECK(data.Splat[0] == 255);
    CHECK(data.Splat[1] == 0);
    CHECK(data.Splat[data.Splat.size() - 4] == 255);
    CHECK(data.Layers.size() == TERRAIN_LAYER_COUNT);
}

TEST_CASE("Terrain resolutions are 2^n + 1 for heights and powers of two for the splat")
{
    CHECK(IsValidTerrainResolution(33));
    CHECK(IsValidTerrainResolution(129));
    CHECK(IsValidTerrainResolution(1025));
    CHECK_FALSE(IsValidTerrainResolution(17));
    CHECK_FALSE(IsValidTerrainResolution(128));
    CHECK_FALSE(IsValidTerrainResolution(2049));
    CHECK(IsValidSplatResolution(16));
    CHECK(IsValidSplatResolution(4096));
    CHECK_FALSE(IsValidSplatResolution(8));
    CHECK_FALSE(IsValidSplatResolution(100));
}

TEST_CASE("A terrain written and read back gives the same text and heights within one step")
{
    const TerrainData       data   = MakeVariedTerrain();
    const std::string       text   = WriteTerrain(data);
    const TerrainReadResult result = ParseTerrain(text);
    REQUIRE_MESSAGE(result.Data, result.Error);

    const TerrainData& read = *result.Data;
    CHECK(read.Size == data.Size);
    CHECK(read.Resolution == data.Resolution);
    CHECK(read.HeightScale == data.HeightScale);
    CHECK(read.SplatResolution == data.SplatResolution);
    CHECK(read.Layers == data.Layers);
    CHECK(read.Splat == data.Splat);
    REQUIRE(read.Heights.size() == data.Heights.size());
    for (size_t i = 0; i < data.Heights.size(); ++i)
        CHECK(std::abs(read.Heights[i] - data.Heights[i]) <= data.HeightScale / 65535.0f);

    CHECK(WriteTerrain(read) == text);
}

TEST_CASE("Writing a terrain clamps its heights and repairs grids of the wrong size")
{
    TerrainData data = MakeVariedTerrain();
    data.Heights[0]  = -5.0f;
    data.Heights[1]  = 500.0f;
    TerrainReadResult result = ParseTerrain(WriteTerrain(data));
    REQUIRE(result.Data);
    CHECK(result.Data->Heights[0] == 0.0f);
    CHECK(result.Data->Heights[1] == data.HeightScale);

    data.Heights.clear();
    data.Splat.resize(3);
    result = ParseTerrain(WriteTerrain(data));
    REQUIRE(result.Data);
    CHECK(result.Data->Heights == std::vector<float>(33 * 33, 0.0f));
    CHECK(result.Data->Splat[0] == 255);
    CHECK(result.Data->Splat[1] == 0);
}

TEST_CASE("A malformed terrain file fails with one message naming the problem")
{
    const std::string text = WriteTerrain(MakeVariedTerrain());

    struct Case
    {
        std::string Text;
        std::string Expected;
    };
    const Case cases[] = {
        { "[1, 2", "" }, // yaml-cpp's own message
        { "- 1\n- 2\n", "is not a map" },
        { ReplaceLine(text, "Version", ""), "Version is missing" },
        { ReplaceLine(text, "Version", "Version: 2"), "Version 2 is not supported" },
        { text + "Extra: 1\n", "unknown key 'Extra'" },
        { ReplaceLine(text, "Splat:", ""), "Splat is missing" },
        { ReplaceLine(text, "Size", "Size: -1"), "Size:" },
        { ReplaceLine(text, "Size", "Size: big"), "'big' is not a number" },
        { ReplaceLine(text, "HeightScale", "HeightScale: .nan"), "is not a finite number" },
        { ReplaceLine(text, "Resolution", "Resolution: 64"), "Resolution: 64 is not 2^n + 1" },
        { ReplaceLine(text, "Resolution", "Resolution: 32.5"), "is not a whole number" },
        { ReplaceLine(text, "SplatResolution", "SplatResolution: 20"), "SplatResolution: 20 is not a power of two" },
        { ReplaceLine(text, "Heights", "Heights: AAAA"), "Heights: holds 3 bytes" },
        { ReplaceLine(text, "Heights", "Heights: not*base64"), "Heights: not base64" },
        { ReplaceLine(text, "Resolution", "Resolution: 65"), "Heights: holds" },
    };
    for (const Case& c : cases)
    {
        const TerrainReadResult result = ParseTerrain(c.Text);
        CHECK_FALSE(result.Data);
        CHECK_FALSE(result.Error.empty());
        CHECK_MESSAGE(result.Error.find(c.Expected) != std::string::npos, result.Error);
    }

    TerrainData oneLayerMissing = MakeVariedTerrain();
    std::string layers          = WriteTerrain(oneLayerMissing);
    const size_t lastLayer      = layers.rfind("  - BaseColor");
    const size_t heights        = layers.find("Heights:");
    layers.erase(lastLayer, heights - lastLayer);
    TerrainReadResult result = ParseTerrain(layers);
    CHECK_FALSE(result.Data);
    CHECK(result.Error.find("exactly 4 layers") != std::string::npos);

    result = ParseTerrain(ReplaceLine(text, "    Tiling", "    Tiling: x"));
    CHECK_FALSE(result.Data);
    CHECK(result.Error.find("layer 0, Tiling") != std::string::npos);
}
