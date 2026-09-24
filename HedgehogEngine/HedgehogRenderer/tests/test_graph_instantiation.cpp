#include "HedgehogRenderer/Graph/GraphAssetParser.hpp"
#include "HedgehogRenderer/Graph/GraphAssetVocabulary.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "GraphPlanOracle.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr size_t ARENA_BYTES = 64 * 1024;

    // A representative scene graph: a depth prepass, a shadow pass, a forward pass that reads
    // both, and a debug overlay nobody reads, which the compiler must cull.
    const char* const SCENE_ASSET = R"(
version: 1
outputs:
  - { slot: 0, name: color, format: R16G16B16A16Float, size: 'Absolute(1280, 720)' }
  - { slot: 1, name: depth, format: D32Float,          size: 'Absolute(1280, 720)' }
resources:
  - { name: shadowMap,    format: D32Float,      size: 'Absolute(2048, 2048)' }
  - { name: debugOverlay, format: R8G8B8A8Unorm, size: 'Absolute(1280, 720)' }
passes:
  - type: DepthPrepass
    name: Prepass
    bindings: { depth: depth }
  - type: Shadow
    name: SunShadow
    bindings: { shadowMap: shadowMap }
  - type: Forward
    name: MainForward
    bindings: { color: color, depth: depth, shadow: shadowMap }
    parameters: { cullBackFaces: 'true' }
  - type: DebugOverlay
    name: Overlay
    bindings: { target: debugOverlay }
)";

    struct TargetData
    {
        RGTexture Target{};
    };

    // ── Test pass builders: stand-ins for the real ones HE-80/HE-81 register ──

    void BuildDepthPrepass(RenderGraphRuntime& graph, PassInvocation& invocation)
    {
        graph.AddPass<TargetData>(invocation.GetName(),
            [&](RGPassBuilder& pass, TargetData& data)
            {
                data.Target = pass.DepthTarget(invocation.GetSlot("depth"));
                invocation.SetSlot("depth", data.Target);
            },
            [](TargetData&, RHI::IRHICommandList&) {});
    }

    void BuildShadow(RenderGraphRuntime& graph, PassInvocation& invocation)
    {
        graph.AddPass<TargetData>(invocation.GetName(),
            [&](RGPassBuilder& pass, TargetData& data)
            {
                data.Target = pass.DepthTarget(invocation.GetSlot("shadowMap"));
                invocation.SetSlot("shadowMap", data.Target);
            },
            [](TargetData&, RHI::IRHICommandList&) {});
    }

    void BuildForward(RenderGraphRuntime& graph, PassInvocation& invocation)
    {
        const bool cullBackFaces = ResolveFlag(invocation.GetParameter("cullBackFaces").value_or("true")).value();
        graph.AddPass<TargetData>(invocation.GetName(),
            [&](RGPassBuilder& pass, TargetData& data)
            {
                pass.DepthReadOnly(invocation.GetSlot("depth"));
                pass.SampleTexture(invocation.GetSlot("shadow"));
                data.Target = pass.ColorTarget(invocation.GetSlot("color"));
                invocation.SetSlot("color", data.Target);
            },
            [cullBackFaces](TargetData&, RHI::IRHICommandList&) { (void)cullBackFaces; });
    }

    void BuildDebugOverlay(RenderGraphRuntime& graph, PassInvocation& invocation)
    {
        graph.AddPass<TargetData>(invocation.GetName(),
            [&](RGPassBuilder& pass, TargetData& data)
            {
                data.Target = pass.ColorTarget(invocation.GetSlot("target"));
                invocation.SetSlot("target", data.Target);
            },
            [](TargetData&, RHI::IRHICommandList&) {});
    }

    PassBuilderRegistry MakeRegistry()
    {
        PassBuilderRegistry registry;
        CHECK(registry.Register("DepthPrepass", { { "depth" }, {}, &BuildDepthPrepass }));
        CHECK(registry.Register("Shadow", { { "shadowMap" }, {}, &BuildShadow }));
        CHECK(registry.Register("Forward", { { "color", "depth", "shadow" },
                                             { { "cullBackFaces", PassParameterKind::Flag } }, &BuildForward }));
        CHECK(registry.Register("DebugOverlay", { { "target" }, {}, &BuildDebugOverlay }));
        return registry;
    }

    GraphAsset ParseAsset(const char* yaml)
    {
        const GraphAssetParseResult parsed = GraphAssetParser{}.Parse(yaml);
        REQUIRE(parsed.Success);
        return parsed.Asset;
    }

    RGTexture Declare(RenderGraphRuntime& graph, const char* name, RHI::Format format, uint32_t width, uint32_t height)
    {
        return graph.CreateTexture({ name, format, RGSizePolicy::MakeAbsolute(width, height), DefaultTextureUsage(format) });
    }

    bool AnyErrorMentions(const GraphInstantiationResult& result, const std::string& text)
    {
        for (const auto& error : result.Errors)
            if (error.Message.find(text) != std::string::npos)
                return true;
        for (const auto& error : result.Errors)
            MESSAGE("reported: " << error.Message);
        return false;
    }
}

TEST_CASE("Oracle: a graph instantiated from an asset compiles to the same plan as its hand-written C++ twin")
{
    const PassBuilderRegistry registry = MakeRegistry();

    TestDevice assetDevice;
    RenderGraphRuntime fromAsset(assetDevice, ARENA_BYTES);
    REQUIRE(GraphInstantiator(registry).Instantiate(ParseAsset(SCENE_ASSET), fromAsset).Success);

    // The twin, written directly against the builder API with no registry and no asset.
    // Resources are declared in a different order on purpose: the plan is keyed by name.
    TestDevice handDevice;
    RenderGraphRuntime byHand(handDevice, ARENA_BYTES);
    const RGTexture shadowMap    = Declare(byHand, "shadowMap", RHI::Format::D32Float, 2048, 2048);
    const RGTexture debugOverlay = Declare(byHand, "debugOverlay", RHI::Format::R8G8B8A8Unorm, 1280, 720);
    const RGTexture depth        = Declare(byHand, "depth", RHI::Format::D32Float, 1280, 720);
    const RGTexture color        = Declare(byHand, "color", RHI::Format::R16G16B16A16Float, 1280, 720);

    RGTexture depthWritten, shadowWritten, colorWritten;
    const auto noExecute = [](TargetData&, RHI::IRHICommandList&) {};
    byHand.AddPass<TargetData>("Prepass",
        [&](RGPassBuilder& pass, TargetData& data) { depthWritten = data.Target = pass.DepthTarget(depth); }, noExecute);
    byHand.AddPass<TargetData>("SunShadow",
        [&](RGPassBuilder& pass, TargetData& data) { shadowWritten = data.Target = pass.DepthTarget(shadowMap); },
        noExecute);
    byHand.AddPass<TargetData>("MainForward",
        [&](RGPassBuilder& pass, TargetData& data)
        {
            pass.DepthReadOnly(depthWritten);
            pass.SampleTexture(shadowWritten);
            colorWritten = data.Target = pass.ColorTarget(color);
        },
        noExecute);
    byHand.AddPass<TargetData>("Overlay",
        [&](RGPassBuilder& pass, TargetData& data) { data.Target = pass.ColorTarget(debugOverlay); }, noExecute);
    byHand.BindOutput(byHand.AddOutputSlot("color", RHI::Format::R16G16B16A16Float,
                                           RGSizePolicy::MakeAbsolute(1280, 720)), colorWritten);
    byHand.BindOutput(byHand.AddOutputSlot("depth", RHI::Format::D32Float,
                                           RGSizePolicy::MakeAbsolute(1280, 720)), depthWritten);

    const std::string assetPlan = DescribeCompiledPlan(fromAsset.GetDescription());
    const std::string handPlan  = DescribeCompiledPlan(byHand.GetDescription());
    CHECK(assetPlan == handPlan);

    // Guard against two equally broken plans comparing equal.
    CHECK(assetPlan.find("compile failed") == std::string::npos);
    CHECK(assetPlan.find("pass MainForward") != std::string::npos);
    CHECK(assetPlan.find("pass Overlay") == std::string::npos); // culled: nothing reads it
}

TEST_CASE("The loader has no capability a C++ caller of the pass builders lacks")
{
    // A C++ caller drives the very same registered build functions with PassInvocations it fills
    // itself, exactly as GraphInstantiator does. Identical plans mean the loader added nothing.
    const PassBuilderRegistry registry = MakeRegistry();

    TestDevice assetDevice;
    RenderGraphRuntime fromAsset(assetDevice, ARENA_BYTES);
    REQUIRE(GraphInstantiator(registry).Instantiate(ParseAsset(SCENE_ASSET), fromAsset).Success);

    TestDevice cppDevice;
    RenderGraphRuntime fromCpp(cppDevice, ARENA_BYTES);
    RGTexture color = Declare(fromCpp, "color", RHI::Format::R16G16B16A16Float, 1280, 720);
    RGTexture depth = Declare(fromCpp, "depth", RHI::Format::D32Float, 1280, 720);
    RGTexture shadowMap = Declare(fromCpp, "shadowMap", RHI::Format::D32Float, 2048, 2048);
    const RGTexture debugOverlay = Declare(fromCpp, "debugOverlay", RHI::Format::R8G8B8A8Unorm, 1280, 720);

    PassInvocation prepass("Prepass");
    prepass.SetSlot("depth", depth);
    registry.Find("DepthPrepass")->Build(fromCpp, prepass);
    depth = prepass.GetSlot("depth");

    PassInvocation shadow("SunShadow");
    shadow.SetSlot("shadowMap", shadowMap);
    registry.Find("Shadow")->Build(fromCpp, shadow);
    shadowMap = shadow.GetSlot("shadowMap");

    PassInvocation forward("MainForward");
    forward.SetSlot("color", color);
    forward.SetSlot("depth", depth);
    forward.SetSlot("shadow", shadowMap);
    forward.SetParameter("cullBackFaces", "true");
    registry.Find("Forward")->Build(fromCpp, forward);
    color = forward.GetSlot("color");

    PassInvocation overlay("Overlay");
    overlay.SetSlot("target", debugOverlay);
    registry.Find("DebugOverlay")->Build(fromCpp, overlay);

    fromCpp.BindOutput(fromCpp.AddOutputSlot("color", RHI::Format::R16G16B16A16Float,
                                             RGSizePolicy::MakeAbsolute(1280, 720)), color);
    fromCpp.BindOutput(fromCpp.AddOutputSlot("depth", RHI::Format::D32Float,
                                             RGSizePolicy::MakeAbsolute(1280, 720)), depth);

    CHECK(DescribeCompiledPlan(fromAsset.GetDescription()) == DescribeCompiledPlan(fromCpp.GetDescription()));
}

TEST_CASE("Semantic errors are rejected before anything is declared, naming the pass and slot")
{
    struct RejectionCase
    {
        const char* Passes;
        const char* ExpectedMessage;
    };

    const RejectionCase cases[] =
    {
        { "  - { type: Bloom, name: Glow, bindings: { target: color } }\n",
          "pass 'Glow': unknown pass type 'Bloom'" },
        { "  - { type: DebugOverlay, name: Overlay, bindings: { target: color, extra: color } }\n",
          "pass 'Overlay': pass type 'DebugOverlay' has no slot 'extra'" },
        { "  - { type: DebugOverlay, name: Overlay }\n",
          "pass 'Overlay': required slot 'target' of pass type 'DebugOverlay' is not bound" },
        { "  - { type: DebugOverlay, name: Overlay, bindings: { target: nowhere } }\n",
          "pass 'Overlay': slot 'target' is bound to 'nowhere', which is not a declared output or resource" },
        { "  - { type: DebugOverlay, name: Overlay, bindings: { target: color }, parameters: { tint: red } }\n",
          "pass 'Overlay': pass type 'DebugOverlay' has no parameter 'tint'" },
        { "  - type: Forward\n    name: Main\n    bindings: { color: color, depth: color, shadow: color }\n"
          "    parameters: { cullBackFaces: 'yes' }\n",
          "pass 'Main': parameter 'cullBackFaces' value 'yes' is not a valid flag (true or false)" },
    };

    const PassBuilderRegistry registry = MakeRegistry();
    for (const RejectionCase& rejection : cases)
    {
        CAPTURE(rejection.Passes);
        const std::string yaml = std::string("version: 1\noutputs:\n")
            + "  - { slot: 0, name: color, format: R8G8B8A8Unorm, size: 'Absolute(64, 64)' }\n"
            + "passes:\n" + rejection.Passes;

        TestDevice device;
        RenderGraphRuntime graph(device, ARENA_BYTES);
        const GraphInstantiationResult result = GraphInstantiator(registry).Instantiate(ParseAsset(yaml.c_str()), graph);

        CHECK_FALSE(result.Success);
        CHECK(AnyErrorMentions(result, rejection.ExpectedMessage));
        CHECK(graph.GetDescription().Resources.empty()); // nothing declared
        CHECK(graph.GetDescription().Passes.empty());
    }
}

TEST_CASE("An output no pass writes is rejected with the slot named, and the graph cannot compile")
{
    const PassBuilderRegistry registry = MakeRegistry();
    TestDevice device;
    RenderGraphRuntime graph(device, ARENA_BYTES);

    const GraphInstantiationResult result = GraphInstantiator(registry).Instantiate(ParseAsset(
        "version: 1\noutputs:\n"
        "  - { slot: 0, name: color, format: R8G8B8A8Unorm, size: 'Absolute(64, 64)' }\n"
        "  - { slot: 1, name: unused, format: R8G8B8A8Unorm, size: 'Absolute(64, 64)' }\n"
        "passes:\n  - { type: DebugOverlay, name: Overlay, bindings: { target: color } }\n"), graph);

    CHECK_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].SlotName == "unused");
    CHECK(AnyErrorMentions(result, "output slot 1 ('unused') is never written by any pass"));
    CHECK(DescribeCompiledPlan(graph.GetDescription()).rfind("compile failed", 0) == 0);
}

TEST_CASE("The view's output contract is checked slot by slot")
{
    const PassBuilderRegistry registry = MakeRegistry();
    const GraphAsset asset = ParseAsset(
        "version: 1\noutputs:\n"
        "  - { slot: 0, name: color, format: R8G8B8A8Unorm, size: 'RelativeToResult(1.0)' }\n"
        "passes:\n  - { type: DebugOverlay, name: Overlay, bindings: { target: color } }\n");

    const auto instantiate = [&](const std::vector<GraphOutputRequirement>& required)
    {
        TestDevice device;
        RenderGraphRuntime graph(device, ARENA_BYTES);
        return GraphInstantiator(registry).Instantiate(asset, graph, &required);
    };

    CHECK(instantiate({ { RHI::Format::R8G8B8A8Unorm, RGSizePolicy::MakeRelativeToResult(1.0f) } }).Success);

    const GraphInstantiationResult wrongFormat =
        instantiate({ { RHI::Format::R16G16B16A16Float, RGSizePolicy::MakeRelativeToResult(1.0f) } });
    CHECK(AnyErrorMentions(wrongFormat, "output slot 0 ('color'): format does not match the view's target"));

    const GraphInstantiationResult wrongSize =
        instantiate({ { RHI::Format::R8G8B8A8Unorm, RGSizePolicy::MakeRelativeToResult(0.5f) } });
    CHECK(AnyErrorMentions(wrongSize, "output slot 0 ('color'): size policy does not match the view's target"));

    const GraphInstantiationResult wrongCount = instantiate({});
    CHECK(AnyErrorMentions(wrongCount, "graph declares 1 output slot(s) but the view binds 0"));
}

TEST_CASE("Registering a pass type twice keeps the first registration")
{
    PassBuilderRegistry registry;
    CHECK(registry.Register("Shadow", { { "shadowMap" }, {}, &BuildShadow }));
    CHECK_FALSE(registry.Register("Shadow", { { "other" }, {}, &BuildDepthPrepass }));
    REQUIRE(registry.Find("Shadow") != nullptr);
    CHECK(registry.Find("Shadow")->Build == &BuildShadow);
    CHECK(registry.Find("Missing") == nullptr);
}
