#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "FileSystem/tests/test_helpers.hpp"
#include "GraphPlanOracle.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <chrono>
#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr size_t ARENA_BYTES = 64 * 1024;

    std::filesystem::path ShippedGraph(const char* name)
    {
        return std::filesystem::path(HH_GRAPH_ASSET_DIR) / (std::string(name) + ".graph");
    }

    PassBuilderRegistry MakeEngineRegistry()
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        return registry;
    }

    struct TargetData
    {
        RGTexture Target{};
    };

    constexpr auto NO_EXECUTE = [](TargetData&, RHI::IRHICommandList&) {};

    RGTexture Declare(RenderGraphRuntime& graph, const char* name, RHI::Format format, const RGSizePolicy& size)
    {
        return graph.CreateTexture({ name, format, size, DefaultTextureUsage(format) });
    }

    // Stands in for the shared phase's outputs when a view graph is compiled on its own.
    GraphImports ImportsFor(RenderGraphRuntime& graph, const GraphAsset& asset)
    {
        GraphImports imports;
        for (const GraphAssetImport& import : asset.Imports)
            imports.emplace(import.Name, graph.ImportTexture(import.Name, import.Format, true));
        return imports;
    }

    // Hand-written twin of scene.graph and game.graph, straight against the builder API.
    void BuildViewGraphByHand(RenderGraphRuntime& graph)
    {
        const RGSizePolicy full = RGSizePolicy::MakeRelativeToResult(1.0f);
        const RGTexture shadowAtlas = graph.ImportTexture("shadowAtlas", RHI::Format::D32Float, true);
        const RGTexture depth       = Declare(graph, "depth", RHI::Format::D32Float, full);
        const RGTexture color       = Declare(graph, "color", RHI::Format::R16G16B16A16Unorm, full);

        RGTexture depthWritten, colorWritten;
        graph.AddPass<TargetData>("DepthPrepass",
            [&](RGPassBuilder& pass, TargetData& data) { depthWritten = data.Target = pass.DepthTarget(depth); },
            NO_EXECUTE);
        graph.AddPass<TargetData>("Forward",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(depthWritten);
                pass.SampleTexture(shadowAtlas);
                colorWritten = data.Target = pass.ColorTarget(color);
            },
            NO_EXECUTE);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm, full), colorWritten);
    }

    // Hand-written twin of result.graph.
    void BuildResultGraphByHand(RenderGraphRuntime& graph)
    {
        const RGSizePolicy swapchain = RGSizePolicy::MakeRelativeToSwapchain(1.0f);
        const RGTexture main = Declare(graph, "main", RHI::Format::B8G8R8A8Srgb, swapchain);

        RGTexture mainWritten;
        graph.AddPass<TargetData>("Ui",
            [&](RGPassBuilder& pass, TargetData& data) { mainWritten = data.Target = pass.ColorTarget(main); },
            NO_EXECUTE);
        graph.BindOutput(graph.AddOutputSlot("main", RHI::Format::B8G8R8A8Srgb, swapchain), mainWritten);
    }

    std::string PlanOf(const PassBuilderRegistry& registry, const GraphAsset& asset)
    {
        TestDevice device;
        RenderGraphRuntime graph(device, ARENA_BYTES);
        const GraphImports imports = ImportsFor(graph, asset);
        const GraphInstantiationResult result = GraphInstantiator(registry).Instantiate(asset, graph, nullptr, &imports);
        for (const auto& error : result.Errors)
            MESSAGE("instantiation: " << error.Message);
        REQUIRE(result.Success);
        return DescribeCompiledPlan(graph.GetDescription());
    }

    std::string PlanOf(void (*buildByHand)(RenderGraphRuntime&))
    {
        TestDevice device;
        RenderGraphRuntime graph(device, ARENA_BYTES);
        buildByHand(graph);
        return DescribeCompiledPlan(graph.GetDescription());
    }

    // Moves a file's write time forward, so Poll() sees the change regardless of the file
    // system's timestamp resolution.
    void Touch(const std::filesystem::path& file, int secondsAhead)
    {
        std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(secondsAhead));
    }

    const char* const SMALL_GRAPH = R"(
version: 1
outputs:
  - { slot: 0, name: main, format: B8G8R8A8Srgb, size: RelativeToSwapchain(1.0) }
passes:
  - { type: Ui, name: Ui, bindings: { target: main } }
)";

    const char* const SMALL_GRAPH_WITH_SHADOW = R"(
version: 1
outputs:
  - { slot: 0, name: main, format: B8G8R8A8Srgb, size: RelativeToSwapchain(1.0) }
resources:
  - { name: shadowMap, format: D32Float, size: 'Absolute(1024, 1024)' }
passes:
  - { type: Shadow, name: Shadow, bindings: { shadowMap: shadowMap } }
  - { type: Ui, name: Ui, bindings: { target: main } }
)";
}

TEST_CASE("The shipped graphs load, validate and instantiate clean")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);

    for (const char* name : { "scene", "game", "result" })
    {
        CAPTURE(name);
        CHECK(library.Register(name, ShippedGraph(name)));
        CHECK(library.GetLastError(name).empty());
        REQUIRE(library.Find(name) != nullptr);
        CHECK(PlanOf(registry, *library.Find(name)).find("compile failed") == std::string::npos);
    }
}

TEST_CASE("Oracle: each shipped graph compiles to the same plan as its hand-written C++ twin")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);
    REQUIRE(library.Register("scene", ShippedGraph("scene")));
    REQUIRE(library.Register("game", ShippedGraph("game")));
    REQUIRE(library.Register("result", ShippedGraph("result")));

    const std::string viewTwin = PlanOf(&BuildViewGraphByHand);
    CHECK(viewTwin.find("compile failed") == std::string::npos);
    CHECK(PlanOf(registry, *library.Find("scene")) == viewTwin);
    CHECK(PlanOf(registry, *library.Find("game")) == viewTwin);

    const std::string resultTwin = PlanOf(&BuildResultGraphByHand);
    CHECK(resultTwin.find("compile failed") == std::string::npos);
    CHECK(PlanOf(registry, *library.Find("result")) == resultTwin);
}

TEST_CASE("Editing a graph asset replaces it, so the next frame builds the new graph")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const auto file = dir.WriteFile("custom.graph", SMALL_GRAPH);

    GraphAssetLibrary library(registry);
    REQUIRE(library.Register("custom", file));
    const std::string before = PlanOf(registry, *library.Find("custom"));

    CHECK(library.Poll().empty()); // unchanged file: nothing reloads

    dir.WriteFile("custom.graph", SMALL_GRAPH_WITH_SHADOW);
    Touch(file, 2);
    CHECK(library.Poll() == std::vector<std::string>{ "custom" });
    CHECK(library.GetLastError("custom").empty());

    const std::string after = PlanOf(registry, *library.Find("custom"));
    CHECK(after != before);
    CHECK(after.find("pass Shadow") == std::string::npos); // Shadow's output is unread: culled
    CHECK(after.find("texture shadowMap") != std::string::npos);
}

TEST_CASE("A broken edit logs a named error and falls back to the last known-good graph")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const auto file = dir.WriteFile("custom.graph", SMALL_GRAPH);

    GraphAssetLibrary library(registry);
    REQUIRE(library.Register("custom", file));
    const std::string knownGood = PlanOf(registry, *library.Find("custom"));

    SUBCASE("malformed YAML")
    {
        dir.WriteFile("custom.graph", "version: 1\npasses:\n  - { type: Ui, name: Ui, bindings: { target: main }\n");
        Touch(file, 2);
        CHECK(library.Poll().empty());
        const std::string error(library.GetLastError("custom"));
        CHECK(error.find("graph 'custom'") != std::string::npos);
        CHECK(error.find("custom.graph") != std::string::npos);
        CHECK(error.find("is malformed; keeping the last known-good version") != std::string::npos);
    }
    SUBCASE("typo in a pass type")
    {
        dir.WriteFile("custom.graph", std::string(SMALL_GRAPH).replace(std::string(SMALL_GRAPH).find("type: Ui"), 8, "type: UI"));
        Touch(file, 2);
        CHECK(library.Poll().empty());
        const std::string error(library.GetLastError("custom"));
        CHECK(error.find("is invalid; keeping the last known-good version") != std::string::npos);
        CHECK(error.find("pass 'Ui': unknown pass type 'UI'") != std::string::npos);
    }

    // The view still renders: its graph is the known-good one, unchanged.
    REQUIRE(library.Find("custom") != nullptr);
    CHECK(PlanOf(registry, *library.Find("custom")) == knownGood);

    // Fixing the file recovers on the next Poll().
    dir.WriteFile("custom.graph", SMALL_GRAPH_WITH_SHADOW);
    Touch(file, 4);
    CHECK(library.Poll() == std::vector<std::string>{ "custom" });
    CHECK(library.GetLastError("custom").empty());
}

TEST_CASE("A graph that never loaded has nothing to fall back to until it is fixed")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const auto file = dir.WriteFile("custom.graph", "version: 3\n");

    GraphAssetLibrary library(registry);
    CHECK_FALSE(library.Register("custom", file));
    CHECK(library.Find("custom") == nullptr);
    CHECK(std::string(library.GetLastError("custom")).find("no known-good version to fall back to")
          != std::string::npos);

    dir.WriteFile("custom.graph", SMALL_GRAPH);
    Touch(file, 2);
    CHECK(library.Poll() == std::vector<std::string>{ "custom" });
    CHECK(library.Find("custom") != nullptr);
}
