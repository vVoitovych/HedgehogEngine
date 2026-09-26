#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"
#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/GraphAssetWriter.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include "doctest/doctest/doctest.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace Renderer;

namespace
{
    GraphAsset MakeSmallAsset()
    {
        GraphAsset asset;
        asset.Version = 2;
        asset.Outputs.push_back({ 0, "color", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeRelativeToResult(1.0f) });
        asset.Resources.push_back({ "depth", RHI::Format::D32Float, RGSizePolicy::MakeRelativeToResult(1.0f) });
        asset.Imports.push_back({ "shadowAtlas", RHI::Format::D32Float });

        GraphAssetPass forward;
        forward.Type       = "Forward";
        forward.Name       = "Forward";
        forward.Bindings   = { { "color", "color" }, { "depth", "depth" }, { "shadowMap", "shadowAtlas" } };
        forward.Parameters = { { "cullBackFaces", "true" } };
        asset.Passes.push_back(forward);
        return asset;
    }

    // Every field kind with values that stress the round trip: every format, all three size policy
    // kinds with awkward scales, several outputs, and strings a YAML emitter has to quote.
    GraphAsset MakeEverythingAsset()
    {
        GraphAsset asset;
        asset.Version = 2;

        uint32_t slot = 0;
        for (const FormatVocabularyEntry& entry : GetFormatVocabulary())
        {
            const RGSizePolicy size = slot % 3 == 0 ? RGSizePolicy::MakeAbsolute(64 + slot, 32)
                                    : slot % 3 == 1 ? RGSizePolicy::MakeRelativeToResult(1.0f / static_cast<float>(slot + 1))
                                                    : RGSizePolicy::MakeRelativeToSwapchain(0.1f * static_cast<float>(slot));
            asset.Outputs.push_back({ slot, "out" + std::to_string(slot), entry.Value, size });
            ++slot;
        }
        asset.Resources.push_back({ "half", RHI::Format::R16Float, RGSizePolicy::MakeRelativeToResult(0.5f) });
        asset.Resources.push_back({ "fixed", RHI::Format::R8Unorm, RGSizePolicy::MakeAbsolute(4096, 4096) });
        asset.Imports.push_back({ "shadowAtlas", RHI::Format::D32Float });
        asset.Imports.push_back({ "history", RHI::Format::R16G16B16A16Float });

        GraphAssetPass pass;
        pass.Type       = "Custom";
        pass.Name       = "Custom pass: #1";
        pass.Bindings   = { { "b", "half" }, { "a", "fixed" }, { "c", "shadowAtlas" } };
        pass.Parameters = { { "flag", "false" }, { "text", "value: with colon, and 'quotes'" }, { "number", "007" },
                            { "yes", "yes" } };
        asset.Passes.push_back(pass);

        GraphAssetPass bare;
        bare.Type = "Ui";
        bare.Name = "Ui";
        asset.Passes.push_back(bare);
        return asset;
    }

    GraphAsset ReadBack(const std::string& text)
    {
        const GraphAssetParseResult parsed = GraphAssetParser{}.Parse(text);
        for (const GraphAssetError& error : parsed.Errors)
            MESSAGE("parse: " << error.Message);
        REQUIRE(parsed.Success);
        return parsed.Asset;
    }

    std::string ReadFile(const std::filesystem::path& file)
    {
        std::ifstream in(file, std::ios::binary);
        std::ostringstream content;
        content << in.rdbuf();
        return content.str();
    }
}

TEST_CASE("A graph asset is written in the shipped graphs' block layout")
{
    const std::string expected =
        "version: 2\n"
        "outputs:\n"
        "  - slot: 0\n"
        "    name: color\n"
        "    format: R16G16B16A16Unorm\n"
        "    size: RelativeToResult(1.0)\n"
        "resources:\n"
        "  - name: depth\n"
        "    format: D32Float\n"
        "    size: RelativeToResult(1.0)\n"
        "imports:\n"
        "  - name: shadowAtlas\n"
        "    format: D32Float\n"
        "passes:\n"
        "  - type: Forward\n"
        "    name: Forward\n"
        "    bindings:\n"
        "      color: color\n"
        "      depth: depth\n"
        "      shadowMap: shadowAtlas\n"
        "    parameters:\n"
        "      cullBackFaces: true\n";
    CHECK(WriteGraphAsset(MakeSmallAsset()) == expected);
}

TEST_CASE("Writing then parsing gives back an equal asset, and writing that again the same text")
{
    for (const GraphAsset& asset : { MakeSmallAsset(), MakeEverythingAsset() })
    {
        const std::string text = WriteGraphAsset(asset);
        CAPTURE(text);
        const GraphAsset readBack = ReadBack(text);
        CHECK(readBack == asset);
        CHECK(WriteGraphAsset(readBack) == text);
    }
}

TEST_CASE("Empty sections are left out, and imports force the schema version that has them")
{
    GraphAsset asset;
    asset.Version = 1;
    GraphAssetPass ui;
    ui.Type     = "Ui";
    ui.Name     = "Ui";
    ui.Bindings = { { "target", "main" } };
    asset.Outputs.push_back({ 0, "main", RHI::Format::B8G8R8A8Srgb, RGSizePolicy::MakeRelativeToSwapchain(1.0f) });
    asset.Passes.push_back(ui);

    const std::string versionOne = WriteGraphAsset(asset);
    CHECK(versionOne.rfind("version: 1\n", 0) == 0);
    CHECK(versionOne.find("resources") == std::string::npos);
    CHECK(versionOne.find("imports") == std::string::npos);
    CHECK(versionOne.find("parameters") == std::string::npos);
    CHECK(ReadBack(versionOne) == asset);

    // Version 1 cannot declare imports, so an asset that has some is written as version 2.
    asset.Imports.push_back({ "shadowAtlas", RHI::Format::D32Float });
    const std::string withImports = WriteGraphAsset(asset);
    CHECK(withImports.rfind("version: 2\n", 0) == 0);
    CHECK(ReadBack(withImports).Imports == asset.Imports);
}

TEST_CASE("An asset the schema rejects is written as-is, and the parser rejects it on the way back")
{
    GraphAsset asset = MakeSmallAsset();
    SUBCASE("an Undefined format")
    {
        asset.Resources[0].Format = RHI::Format::Undefined;
        CHECK(WriteGraphAsset(asset).find("format: Undefined") != std::string::npos);
    }
    SUBCASE("an empty parameter value")
    {
        asset.Passes[0].Parameters[0].Value.clear();
    }
    CHECK_FALSE(GraphAssetParser{}.Parse(WriteGraphAsset(asset)).Success);
}

TEST_CASE("WriteGraphAssetFile replaces the file whole and leaves no temporary behind")
{
    TempDir dir;
    const std::filesystem::path file = dir.Path() / "custom.graph";
    dir.WriteFile("custom.graph", "version: 1\n");

    std::string error;
    REQUIRE(WriteGraphAssetFile(MakeSmallAsset(), file, &error));
    CHECK(error.empty());
    CHECK(ReadFile(file) == WriteGraphAsset(MakeSmallAsset()));
    CHECK_FALSE(std::filesystem::exists(dir.Path() / "custom.graph.tmp"));

    SUBCASE("a directory that does not exist fails with a message and writes nothing")
    {
        const std::filesystem::path missing = dir.Path() / "missing" / "custom.graph";
        CHECK_FALSE(WriteGraphAssetFile(MakeSmallAsset(), missing, &error));
        CHECK(error.find("missing") != std::string::npos);
        CHECK_FALSE(std::filesystem::exists(missing));
    }
}
