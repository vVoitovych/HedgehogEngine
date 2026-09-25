#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;

namespace
{
    const char* const SCENE_GRAPH = R"(
version: 1
outputs:
  - slot: 1
    name: depth
    format: D32Float
    size: RelativeToResult(1.0)
  - slot: 0
    name: color
    format: R16G16B16A16Float
    size: RelativeToResult(1.0)
resources:
  - name: shadowMap
    format: D32Float
    size: Absolute(2048, 2048)
passes:
  - type: Shadow
    name: SunShadow
    bindings:
      depth: shadowMap
  - type: Forward
    name: MainForward
    bindings:
      color: color
      depth: depth
      shadow: shadowMap
    parameters:
      cullBackFaces: true
)";

    bool AnyErrorMentions(const GraphAssetParseResult& result, const std::string& text)
    {
        for (const auto& error : result.Errors)
            if (error.Message.find(text) != std::string::npos)
                return true;
        // Show what was reported instead, so a failing CHECK is diagnosable from the log alone.
        for (const auto& error : result.Errors)
            MESSAGE("reported: " << error.Message);
        return false;
    }

    GraphAssetParseResult Parse(const std::string& yaml)
    {
        const GraphAssetParser parser;
        return parser.Parse(yaml);
    }
}

TEST_CASE("A well-formed graph asset parses into resolved schema data")
{
    const GraphAssetParseResult result = Parse(SCENE_GRAPH);
    REQUIRE(result.Success);
    CHECK(result.Errors.empty());

    const GraphAsset& asset = result.Asset;
    CHECK(asset.Version == 1);

    SUBCASE("outputs are ordered by slot, not by document order")
    {
        REQUIRE(asset.Outputs.size() == 2);
        CHECK(asset.Outputs[0].Slot == 0);
        CHECK(asset.Outputs[0].Name == "color");
        CHECK(asset.Outputs[0].Format == RHI::Format::R16G16B16A16Float);
        CHECK(asset.Outputs[0].Size.Kind == RGSizePolicyKind::RelativeToResult);
        CHECK(asset.Outputs[1].Slot == 1);
        CHECK(asset.Outputs[1].Name == "depth");
        CHECK(asset.Outputs[1].Format == RHI::Format::D32Float);
    }

    SUBCASE("resources carry their resolved format and size")
    {
        REQUIRE(asset.Resources.size() == 1);
        CHECK(asset.Resources[0].Name == "shadowMap");
        CHECK(asset.Resources[0].Size.Kind == RGSizePolicyKind::Absolute);
        CHECK(asset.Resources[0].Size.Width == 2048);
    }

    SUBCASE("passes keep document order, bindings and untyped parameters")
    {
        REQUIRE(asset.Passes.size() == 2);
        CHECK(asset.Passes[0].Type == "Shadow");
        CHECK(asset.Passes[0].Name == "SunShadow");

        const GraphAssetPass& forward = asset.Passes[1];
        CHECK(forward.Type == "Forward");
        REQUIRE(forward.Bindings.size() == 3);
        CHECK(forward.Bindings[0].Slot == "color");
        CHECK(forward.Bindings[0].Resource == "color");
        CHECK(forward.Bindings[2].Slot == "shadow");
        CHECK(forward.Bindings[2].Resource == "shadowMap");
        REQUIRE(forward.Parameters.size() == 1);
        CHECK(forward.Parameters[0].Name == "cullBackFaces");
        CHECK(forward.Parameters[0].Value == "true");
    }
}

TEST_CASE("An empty graph with only a version is well-formed")
{
    const GraphAssetParseResult result = Parse("version: 1\n");
    REQUIRE(result.Success);
    CHECK(result.Asset.Outputs.empty());
    CHECK(result.Asset.Resources.empty());
    CHECK(result.Asset.Passes.empty());
}

TEST_CASE("An unknown schema version is rejected alone, naming the version")
{
    // The broken pass below must not be reported: under version 3 nothing else is interpretable.
    const GraphAssetParseResult result = Parse("version: 3\npasses:\n  - name: NoType\n");
    REQUIRE_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].Path == "version");
    CHECK(AnyErrorMentions(result, "unsupported graph asset schema version '3' (this build reads versions 1 to 2)"));
}

TEST_CASE("Version 2 declares imports; version 1 still loads but cannot declare them")
{
    const GraphAssetParseResult v2 = Parse(
        "version: 2\n"
        "imports:\n  - { name: shadowAtlas, format: D32Float }\n"
        "passes:\n  - { type: Forward, name: Main, bindings: { shadowMap: shadowAtlas } }\n");
    REQUIRE(v2.Success);
    CHECK(v2.Asset.Version == 2);
    REQUIRE(v2.Asset.Imports.size() == 1);
    CHECK(v2.Asset.Imports[0].Name == "shadowAtlas");
    CHECK(v2.Asset.Imports[0].Format == RHI::Format::D32Float);

    CHECK(Parse("version: 1\npasses: []\n").Success);
    CHECK(AnyErrorMentions(Parse("version: 1\nimports:\n  - { name: a, format: D32Float }\n"),
                           "graph asset: unknown key 'imports'"));
}

TEST_CASE("Malformed documents are rejected with a message naming the offender")
{
    struct RejectionCase
    {
        const char* Yaml;
        const char* ExpectedMessage;
    };

    // Absolute(w, h) needs quoting inside a flow map ({ ... }): YAML splits it at the comma.
    const RejectionCase cases[] =
    {
        // Version
        { "passes: []\n", "graph asset: missing required key 'version'" },
        { "version: one\n", "unsupported graph asset schema version 'one'" },
        // Malformed passes
        { "version: 1\npasses:\n  - name: Lonely\n", "passes[0] ('Lonely'): missing required key 'type'" },
        { "version: 1\npasses:\n  - Forward\n", "passes[0]: malformed pass: expected a map" },
        { "version: 1\npasses:\n  - { type: Forward, name: Main, parameters: { blend: { src: One } } }\n",
          "passes[0] ('Main').parameters: malformed pass: parameter entry 'blend'" },
        { "version: 1\npasses:\n  - { type: Forward, name: Main }\n  - { type: Forward, name: Main }\n",
          "duplicate pass name 'Main', already used by passes[0]" },
        { "version: 1\npasses:\n  - { type: Forward, name: Main, enabled: false }\n",
          "passes[0] ('Main'): unknown key 'enabled'" },
        // yaml-cpp rejects duplicate keys itself while loading, with the offending line.
        { "version: 1\npasses:\n  - type: Forward\n    name: Main\n    bindings:\n      color: a\n      color: b\n",
          "map keys must be unique" },
        // Malformed output slots
        { "version: 1\noutputs:\n  - { slot: first, name: c, format: D32Float, size: RelativeToResult(1.0) }\n",
          "outputs[0].slot: malformed output slot: 'first' is not a non-negative integer" },
        { "version: 1\noutputs:\n  - { slot: 0, name: a, format: D32Float, size: RelativeToResult(1.0) }\n"
          "  - { slot: 0, name: b, format: D32Float, size: RelativeToResult(1.0) }\n",
          "duplicate output slot 0, already declared by outputs[0]" },
        { "version: 1\noutputs:\n  - { slot: 0, name: a, format: D32Float, size: RelativeToResult(1.0) }\n"
          "  - { slot: 2, name: b, format: D32Float, size: RelativeToResult(1.0) }\n",
          "slots must run contiguously from 0, but slot 1 is not declared" },
        // Vocabulary
        { "version: 1\noutputs:\n  - { slot: 0, name: c, format: RGBA16F, size: RelativeToResult(1.0) }\n",
          "outputs[0].format: unknown format 'RGBA16F'" },
        { "version: 1\nresources:\n  - { name: r, format: D32Float, size: RelativeToResult(0) }\n",
          "resources[0].size: unknown size policy 'RelativeToResult(0)'" },
        // Document structure
        { "version: 1\noutputs:\n  - { slot: 0, name: color, format: D32Float, size: RelativeToResult(1.0) }\n"
          "resources:\n  - { name: color, format: D32Float, size: 'Absolute(64, 64)' }\n",
          "duplicate resource name 'color', already declared by outputs (slot 0)" },
        { "version: 1\nconditions: []\n", "graph asset: unknown key 'conditions'" },
        // Imports
        { "version: 2\nimports:\n  - { name: a, format: D32Float, size: 'Absolute(1, 1)' }\n",
          "imports[0]: unknown key 'size'" },
        { "version: 2\nimports:\n  - { format: D32Float }\n", "imports[0]: missing required key 'name'" },
        { "version: 2\nimports:\n  - { name: a, format: D32 }\n", "imports[0].format: unknown format 'D32'" },
        { "version: 2\nresources:\n  - { name: a, format: D32Float, size: 'Absolute(1, 1)' }\n"
          "imports:\n  - { name: a, format: D32Float }\n",
          "duplicate resource name 'a', already declared by resources[0]" },
        { "version: 1\npasses: Forward\n", "passes: must be a sequence" },
        { "version: [1\n", "graph asset is not valid YAML" },
        { "- 1\n- 2\n", "graph asset must be a YAML map" },
    };

    for (const RejectionCase& rejection : cases)
    {
        CAPTURE(rejection.Yaml);
        const GraphAssetParseResult result = Parse(rejection.Yaml);
        CHECK_FALSE(result.Success);
        CHECK(AnyErrorMentions(result, rejection.ExpectedMessage));
    }
}

TEST_CASE("Errors carry the path of the offending node")
{
    CHECK(Parse("version: 1\npasses:\n  - name: Lonely\n").Errors.at(0).Path == "passes[0] ('Lonely')");
    CHECK(Parse("version: 1\noutputs:\n  - { slot: x, name: c, format: D32Float, size: RelativeToResult(1.0) }\n")
              .Errors.at(0).Path == "outputs[0].slot");
}

TEST_CASE("The three malformed cases produce distinct messages")
{
    const GraphAssetParseResult badVersion = Parse("version: 7\n");
    const GraphAssetParseResult badPass    = Parse("version: 1\npasses:\n  - name: Lonely\n");
    const GraphAssetParseResult badSlot    = Parse(
        "version: 1\noutputs:\n  - { slot: x, name: c, format: D32Float, size: RelativeToResult(1.0) }\n");

    REQUIRE_FALSE(badVersion.Errors.empty());
    REQUIRE_FALSE(badPass.Errors.empty());
    REQUIRE_FALSE(badSlot.Errors.empty());
    CHECK(badVersion.Errors[0].Message != badPass.Errors[0].Message);
    CHECK(badPass.Errors[0].Message != badSlot.Errors[0].Message);
    CHECK(badVersion.Errors[0].Message != badSlot.Errors[0].Message);
}

TEST_CASE("Every error in a document is reported, not just the first")
{
    const GraphAssetParseResult result = Parse(
        "version: 1\n"
        "resources:\n  - { name: a, format: Nope, size: 'Absolute(1, 1)' }\n"
        "passes:\n  - { name: NoType }\n");
    REQUIRE_FALSE(result.Success);
    CHECK(result.Errors.size() == 2);
    CHECK(AnyErrorMentions(result, "unknown format 'Nope'"));
    CHECK(AnyErrorMentions(result, "missing required key 'type'"));
}
