#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"
#include "HedgehogRenderer/Graph/GraphDiagnostics.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <filesystem>
#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    PassBuilderRegistry MakeEngineRegistry()
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        return registry;
    }

    GraphAsset LoadShipped(const PassBuilderRegistry& registry, const char* name)
    {
        GraphAssetLibrary library(registry);
        REQUIRE(library.Register(name, std::filesystem::path(HH_GRAPH_ASSET_DIR) / (std::string(name) + ".graph")));
        return *library.Find(name);
    }

    GraphAssetPass& FindPass(GraphAsset& asset, const std::string& name)
    {
        const auto it = std::ranges::find(asset.Passes, name, &GraphAssetPass::Name);
        REQUIRE(it != asset.Passes.end());
        return *it;
    }

    std::string Describe(const std::vector<GraphDiagnostic>& diagnostics)
    {
        std::string text;
        for (const GraphDiagnostic& diagnostic : diagnostics)
            text += "[" + diagnostic.PassName + "|" + diagnostic.SlotName + "|" + diagnostic.ResourceName + "] "
                  + diagnostic.Message + "\n";
        return text;
    }
}

TEST_CASE("The shipped graphs have no diagnostics")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TestDevice device;
    for (const char* name : { "scene", "game", "result" })
    {
        CAPTURE(name);
        const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(LoadShipped(registry, name), registry, device);
        CAPTURE(Describe(diagnostics));
        CHECK(diagnostics.empty());
    }
}

TEST_CASE("Validation problems are tagged with the pass and slot they concern")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TestDevice device;
    GraphAsset asset = LoadShipped(registry, "game");

    SUBCASE("an unbound slot")
    {
        std::erase_if(FindPass(asset, "Forward").Bindings, [](const auto& binding) { return binding.Slot == "depth"; });
        const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(asset, registry, device);
        CAPTURE(Describe(diagnostics));
        REQUIRE(diagnostics.size() == 1);
        CHECK(diagnostics[0].PassName == "Forward");
        CHECK(diagnostics[0].SlotName == "depth");
        CHECK(diagnostics[0].Message.find("depth") != std::string::npos);
    }
    SUBCASE("an unknown pass type")
    {
        FindPass(asset, "DepthPrepass").Type = "Blur";
        const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(asset, registry, device);
        CAPTURE(Describe(diagnostics));
        REQUIRE_FALSE(diagnostics.empty());
        CHECK(diagnostics[0].PassName == "DepthPrepass");
        CHECK(diagnostics[0].Message.find("Blur") != std::string::npos);
    }
    SUBCASE("a binding to a name the graph does not declare")
    {
        FindPass(asset, "Forward").Bindings[0].Resource = "missing";
        const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(asset, registry, device);
        CAPTURE(Describe(diagnostics));
        REQUIRE_FALSE(diagnostics.empty());
        CHECK(diagnostics[0].PassName == "Forward");
        CHECK(diagnostics[0].Message.find("missing") != std::string::npos);
    }
}

TEST_CASE("An output no pass writes is tagged with its slot")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TestDevice device;
    GraphAsset asset = LoadShipped(registry, "game");

    // Forward draws into a transient instead of the output.
    asset.Resources.push_back({ "scratch", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeRelativeToResult(1.0f) });
    std::ranges::find(FindPass(asset, "Forward").Bindings, "color", &GraphAssetBinding::Slot)->Resource = "scratch";

    const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(asset, registry, device);
    CAPTURE(Describe(diagnostics));
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].SlotName == "color");
    CHECK(diagnostics[0].Message.find("never written") != std::string::npos);
}

TEST_CASE("Problems only the whole graph shows come from compiling it, tagged with the pass and resource")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    TestDevice device;
    GraphAsset asset = LoadShipped(registry, "game");

    // A Shadow pass bound to the imported atlas: the binding is valid on its own, so validation
    // passes, but imports are read-only in a view graph and only the compiled graph shows the write.
    GraphAssetPass shadow;
    shadow.Type     = "Shadow";
    shadow.Name     = "Shadow";
    shadow.Bindings = { { "shadowMap", "shadowAtlas" } };
    asset.Passes.insert(asset.Passes.begin(), shadow);

    const std::vector<GraphDiagnostic> diagnostics = DiagnoseGraphAsset(asset, registry, device);
    CAPTURE(Describe(diagnostics));
    REQUIRE_FALSE(diagnostics.empty());
    CHECK(std::ranges::any_of(diagnostics, [](const GraphDiagnostic& diagnostic)
    {
        return diagnostic.PassName == "Shadow" && diagnostic.ResourceName == "shadowAtlas"
            && diagnostic.Message.find("read-only") != std::string::npos;
    }));
}
