#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"
#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"
#include "HedgehogRenderer/Graph/GraphAssetWriter.hpp"
#include "HedgehogRenderer/Graph/GraphReference.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "FileSystem/tests/test_helpers.hpp"
#include "GraphPlanOracle.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <chrono>
#include <string>
#include <utility>

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

    // The passes game.graph and scene.graph share, straight against the builder API: the lit HDR
    // radiance with the skybox behind it, tone mapped onto the colour. Returns the tone-mapped colour
    // and sets depthWritten to the prepass depth.
    RGTexture BuildViewPasses(RenderGraphRuntime& graph, RGTexture& depthWritten)
    {
        const RGSizePolicy full = RGSizePolicy::MakeRelativeToResult(1.0f);
        const RGTexture shadowAtlas = graph.ImportTexture("shadowAtlas", RHI::Format::D32Float, true);
        const RGTexture depth       = Declare(graph, "depth", RHI::Format::D32Float, full);
        const RGTexture hdr         = Declare(graph, "hdr", RHI::Format::R16G16B16A16Float, full);
        const RGTexture color       = Declare(graph, "color", RHI::Format::R16G16B16A16Unorm, full);

        RGTexture hdrWritten;
        graph.AddPass<TargetData>("DepthPrepass",
            [&](RGPassBuilder& pass, TargetData& data) { depthWritten = data.Target = pass.DepthTarget(depth); },
            NO_EXECUTE);
        graph.AddPass<TargetData>("Forward",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(depthWritten);
                pass.SampleTexture(shadowAtlas);
                hdrWritten = data.Target = pass.ColorTarget(hdr);
            },
            NO_EXECUTE);
        RGTexture skyWritten;
        graph.AddPass<TargetData>("Skybox",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(depthWritten);
                skyWritten = data.Target = pass.ColorTarget(hdrWritten);
            },
            NO_EXECUTE);
        RGTexture blendedWritten;
        graph.AddPass<TargetData>("ForwardTransparent",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(depthWritten);
                pass.SampleTexture(shadowAtlas);
                blendedWritten = data.Target = pass.ColorTarget(skyWritten);
            },
            NO_EXECUTE);

        RGTexture colorWritten;
        graph.AddPass<TargetData>("ToneMap",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.SampleTexture(blendedWritten);
                colorWritten = data.Target = pass.ColorTarget(color);
            },
            NO_EXECUTE);
        return colorWritten;
    }

    // Hand-written twin of game.graph: the view's passes, then the game UI over them.
    void BuildViewGraphByHand(RenderGraphRuntime& graph)
    {
        RGTexture       depthWritten;
        const RGTexture litColor = BuildViewPasses(graph, depthWritten);

        RGTexture colorWritten;
        graph.AddPass<TargetData>("GameUi",
            [&](RGPassBuilder& pass, TargetData& data) { colorWritten = data.Target = pass.ColorTarget(litColor); },
            NO_EXECUTE);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm,
                                             RGSizePolicy::MakeRelativeToResult(1.0f)), colorWritten);
    }

    // Hand-written twin of scene.graph: the game view's passes, the selection's mask, then the
    // gizmos over them.
    void BuildSceneGraphByHand(RenderGraphRuntime& graph)
    {
        RGTexture       depthWritten;
        const RGTexture litColor = BuildViewPasses(graph, depthWritten);

        const RGTexture selectionMask =
            Declare(graph, "selectionMask", RHI::Format::R8Unorm, RGSizePolicy::MakeRelativeToResult(1.0f));
        graph.AddPass<TargetData>("SelectionMask",
            [&](RGPassBuilder& pass, TargetData& data) { data.Target = pass.ColorTarget(selectionMask); },
            NO_EXECUTE);

        RGTexture colorWritten;
        graph.AddPass<TargetData>("Gizmo",
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(depthWritten);
                colorWritten = data.Target = pass.ColorTarget(litColor);
            },
            NO_EXECUTE);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm,
                                             RGSizePolicy::MakeRelativeToResult(1.0f)), colorWritten);
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

    const std::string sceneTwin = PlanOf(&BuildSceneGraphByHand);
    CHECK(sceneTwin.find("compile failed") == std::string::npos);
    CHECK(sceneTwin.find("pass Gizmo") != std::string::npos);
    CHECK(PlanOf(registry, *library.Find("scene")) == sceneTwin);

    const std::string gameTwin = PlanOf(&BuildViewGraphByHand);
    CHECK(gameTwin.find("compile failed") == std::string::npos);
    CHECK(gameTwin.find("pass GameUi") != std::string::npos);
    // Debug lines draw in the Gizmo pass, which only the editor's scene view has.
    CHECK(gameTwin.find("pass Gizmo") == std::string::npos);
    CHECK(PlanOf(registry, *library.Find("game")) == gameTwin);

    // Forward renders HDR radiance, the Skybox fills the rest, ForwardTransparent blends over both,
    // ToneMap maps it onto the colour, and the overlays draw after.
    for (const auto& [twin, overlay] : { std::pair{ &sceneTwin, "pass Gizmo" }, std::pair{ &gameTwin, "pass GameUi" } })
    {
        const size_t prepass = twin->find("pass DepthPrepass");
        const size_t forward = twin->find("pass Forward");
        const size_t skybox  = twin->find("pass Skybox");
        const size_t blended = twin->find("pass ForwardTransparent");
        const size_t toneMap = twin->find("pass ToneMap");
        REQUIRE(skybox != std::string::npos);
        REQUIRE(toneMap != std::string::npos);
        CHECK(prepass < forward);
        CHECK(forward < skybox);
        CHECK(skybox < blended);
        CHECK(blended < toneMap);
        CHECK(toneMap < twin->find(overlay));
    }

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

TEST_CASE("A graph directory registers every .graph file under its stem, in sorted order")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);

    SUBCASE("the shipped directory")
    {
        REQUIRE(library.RegisterDirectory(HH_GRAPH_ASSET_DIR));
        CHECK(library.GetNames() == std::vector<std::string>{ "game", "result", "scene" });
        for (const std::string& name : library.GetNames())
        {
            CAPTURE(name);
            CHECK(library.Find(name) != nullptr);
        }
    }
    SUBCASE("other files are ignored, and a broken graph is still listed")
    {
        TempDir dir;
        dir.WriteFile("b.graph", SMALL_GRAPH);
        dir.WriteFile("a.graph", SMALL_GRAPH_WITH_SHADOW);
        dir.WriteFile("broken.graph", "version: 3\n");
        dir.WriteFile("notes.txt", "not a graph");
        dir.WriteFile("c.graph.bak", SMALL_GRAPH);

        REQUIRE(library.RegisterDirectory(dir.Path()));
        CHECK(library.GetNames() == std::vector<std::string>{ "a", "b", "broken" });
        CHECK(library.Find("a") != nullptr);
        CHECK(library.Find("broken") == nullptr);
        CHECK_FALSE(library.GetLastError("broken").empty());
    }
    SUBCASE("a missing directory registers nothing")
    {
        CHECK_FALSE(library.RegisterDirectory(std::filesystem::path(HH_GRAPH_ASSET_DIR) / "missing"));
        CHECK(library.GetNames().empty());
    }
}

TEST_CASE("A graph file added to a watched directory is registered by the next Poll()")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    dir.WriteFile("game.graph", SMALL_GRAPH);

    GraphAssetLibrary library(registry);
    REQUIRE(library.RegisterDirectory(dir.Path()));
    CHECK(library.Poll().empty()); // nothing added: the directory is not even listed

    dir.WriteFile("custom.graph", SMALL_GRAPH_WITH_SHADOW);
    Touch(dir.Path(), 2); // the directory's write time is what Poll() checks
    CHECK(library.Poll() == std::vector<std::string>{ "custom" });
    CHECK(library.GetNames() == std::vector<std::string>{ "custom", "game" });
    REQUIRE(library.Find("custom") != nullptr);
    CHECK(PlanOf(registry, *library.Find("custom")) != PlanOf(registry, *library.Find("game")));

    CHECK(library.Poll().empty()); // registered once, not again
}

TEST_CASE("Oracle: each shipped graph, written and parsed back, is equal and compiles to the same plan")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);
    REQUIRE(library.RegisterDirectory(HH_GRAPH_ASSET_DIR));

    for (const char* name : { "scene", "game", "result" })
    {
        CAPTURE(name);
        const GraphAsset* shipped = library.Find(name);
        REQUIRE(shipped != nullptr);

        const std::string text = WriteGraphAsset(*shipped);
        const GraphAssetParseResult reparsed = GraphAssetParser{}.Parse(text);
        REQUIRE(reparsed.Success);
        CHECK(reparsed.Asset == *shipped);
        CHECK(PlanOf(registry, reparsed.Asset) == PlanOf(registry, *shipped));
        CHECK(WriteGraphAsset(reparsed.Asset) == text);
    }
}

TEST_CASE("A graph file anywhere is loaded by path on first use and hot-reloaded like any other")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const std::filesystem::path file = dir.WriteFile("custom.graph", SMALL_GRAPH);
    const std::string reference = file.generic_string();

    GraphAssetLibrary library(registry);
    CHECK(library.Find(reference) == nullptr); // Find never registers

    const GraphAsset* asset = library.FindOrLoad(reference, nullptr);
    REQUIRE(asset != nullptr);
    CHECK(library.GetFile(reference) == file);

    // Another spelling of the same file finds the same entry rather than registering it twice.
    std::string backslashed = file.string();
    CHECK(library.FindOrLoad(backslashed, nullptr) == asset);
    CHECK(library.GetNames().size() == 1);

    const std::string before = PlanOf(registry, *asset);
    dir.WriteFile("custom.graph", SMALL_GRAPH_WITH_SHADOW);
    Touch(file, 2);
    CHECK(library.Poll().size() == 1);
    CHECK(PlanOf(registry, *library.Find(reference)) != before);
}

TEST_CASE("A virtual graph path is resolved through the caller's resolver")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const std::filesystem::path file = dir.WriteFile("custom.graph", SMALL_GRAPH);

    GraphAssetLibrary library(registry);
    const auto resolve = [&](std::string_view virtualPath) -> std::optional<std::filesystem::path>
    {
        if (virtualPath == "assets://Graphs/custom.graph")
            return file;
        return std::nullopt;
    };
    CHECK(library.FindOrLoad("assets://Graphs/custom.graph", resolve) != nullptr);
    CHECK(library.GetFile("assets://Graphs/custom.graph") == file);

    CHECK(library.FindOrLoad("assets://Graphs/other.graph", resolve) == nullptr);
    CHECK(std::string(library.GetLastError("assets://Graphs/other.graph")).find("does not exist") != std::string::npos);
}

TEST_CASE("A missing graph file is reported, not registered, and picked up once it exists")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const std::filesystem::path file = dir.Path() / "later.graph";
    const std::string reference = file.generic_string();

    GraphAssetLibrary library(registry);
    CHECK(library.FindOrLoad(reference, nullptr) == nullptr);
    CHECK(std::string(library.GetLastError(reference)).find("does not exist") != std::string::npos);
    CHECK(library.GetNames().empty());

    dir.WriteFile("later.graph", SMALL_GRAPH);
    CHECK(library.FindOrLoad(reference, nullptr) != nullptr);
    CHECK(library.GetLastError(reference).empty());
}

TEST_CASE("Names are listed before file references, and an unknown name is never loaded as a file")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TempDir dir;
    const std::filesystem::path file = dir.WriteFile("aaa.graph", SMALL_GRAPH);

    GraphAssetLibrary library(registry);
    REQUIRE(library.FindOrLoad(file.generic_string(), nullptr) != nullptr);
    REQUIRE(library.Register("zzz", file));
    CHECK(library.FindOrLoad("unknown", nullptr) == nullptr);

    const std::vector<std::string>& names = library.GetNames();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "zzz");
    CHECK(names[1] == NormalizeGraphReference(file.generic_string()));
}

