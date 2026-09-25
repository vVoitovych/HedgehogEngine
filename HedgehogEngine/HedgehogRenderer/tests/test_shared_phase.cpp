#include "HedgehogRenderer/Frame/SharedPhase.hpp"
#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <string>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr size_t ARENA_BYTES = 256 * 1024;

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

    size_t CountPasses(const CompiledGraph& graph, const std::string& name)
    {
        return static_cast<size_t>(std::count_if(graph.Passes.begin(), graph.Passes.end(),
                                                 [&](const CompiledPass& pass) { return pass.Name == name; }));
    }

    size_t FirstIndexOf(const CompiledGraph& graph, const std::string& name)
    {
        for (size_t i = 0; i < graph.Passes.size(); ++i)
            if (graph.Passes[i].Name == name)
                return i;
        return graph.Passes.size();
    }

    View MakeView(ViewId id, int32_t priority, bool hasCamera)
    {
        View view;
        view.Id            = id;
        view.Desc.Priority = priority;
        if (hasCamera)
            view.Desc.Camera = HX::RenderCamera{};
        return view;
    }
}

TEST_CASE("The shared phase renders the shadow atlas once however many views import it")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);
    REQUIRE(library.Register("scene", ShippedGraph("scene")));

    for (int viewCount = 1; viewCount <= 3; ++viewCount)
    {
        CAPTURE(viewCount);
        TestDevice         device;
        RenderGraphRuntime graph(device, ARENA_BYTES);
        FakeServices       services;
        SharedPhase        shared(registry);
        const GraphFrameData shadowView = MakeFrame(1);

        const SharedPhaseOutputs outputs = shared.Declare(graph, services, &shadowView, {}, {});
        REQUIRE(outputs.Imports.contains(SHADOW_ATLAS_IMPORT));
        for (int view = 0; view < viewCount; ++view)
        {
            REQUIRE(GraphInstantiator(registry).Instantiate(*library.Find("scene"), graph, nullptr, &outputs.Imports)
                        .Success);
        }

        const CompileResult compiled = GraphCompiler{}.Compile(graph.GetDescription());
        REQUIRE(compiled.Success);
        CHECK(CountPasses(compiled.Graph, "Shadow") == 1);
        CHECK(CountPasses(compiled.Graph, "Forward") == static_cast<size_t>(viewCount));
        CHECK(FirstIndexOf(compiled.Graph, "Shadow") < FirstIndexOf(compiled.Graph, "Forward"));

        // The atlas becomes sampleable once, before the first view reads it; later views find it
        // already in that state. Derived by the compiler, not written by any pass.
        size_t toSampled = 0;
        for (const CompiledPass& pass : compiled.Graph.Passes)
        {
            for (const RGTextureBarrier& barrier : pass.TextureBarriers)
            {
                if (barrier.Id == outputs.Imports.at(SHADOW_ATLAS_IMPORT).Id
                    && barrier.After == RHI::ResourceState::ShaderResource)
                {
                    CHECK(barrier.Before == RHI::ResourceState::DepthWrite);
                    ++toSampled;
                }
            }
        }
        CHECK(toSampled == 1);
    }
}

TEST_CASE("A frame whose views import nothing from the shared phase culls its shadow pass")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    GraphAssetLibrary library(registry);
    REQUIRE(library.Register("result", ShippedGraph("result")));

    TestDevice         device;
    RenderGraphRuntime graph(device, ARENA_BYTES);
    FakeServices       services;
    SharedPhase        shared(registry);
    const GraphFrameData shadowView = MakeFrame(1);

    const SharedPhaseOutputs outputs = shared.Declare(graph, services, &shadowView, {}, {});
    REQUIRE(GraphInstantiator(registry).Instantiate(*library.Find("result"), graph, nullptr, &outputs.Imports).Success);

    const CompileResult compiled = GraphCompiler{}.Compile(graph.GetDescription());
    REQUIRE(compiled.Success);
    CHECK(CountPasses(compiled.Graph, "Shadow") == 0);
    CHECK(CountPasses(compiled.Graph, "Ui") == 1);
}

TEST_CASE("Shadow casters come from the caster mask, never from the view's layer mask")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();

    // The primary view shows layer 0 only. Layer 5 is hidden from it but still casts; layer 3 is
    // excluded from casting.
    std::vector<View> views = { MakeView(1, 10, false), MakeView(2, 0, true) };
    views[1].Desc.LayerMask = 1u;
    REQUIRE(SelectShadowView(views) == &views[1]);

    TestBuffer positions(1024);
    TestBuffer indices(1024);
    const MeshDrawRange      meshes[]    = { { 0, 36, 0 } };
    const HX::RenderInstance instances[] = { Instance(0, 0), Instance(0, 3), Instance(0, 5) };
    GraphFrameData shadowView  = MakeFrame(1);
    shadowView.OpaqueInstances = instances;
    shadowView.Meshes          = meshes;
    shadowView.Positions       = &positions;
    shadowView.Indices         = &indices;

    SharedPhaseSettings settings;
    settings.ShadowAtlasSize  = 512;
    settings.ShadowCasterMask = ~(1u << 3);

    TestDevice         device;
    RenderGraphRuntime graph(device, ARENA_BYTES);
    FakeServices       services;
    SharedPhase        shared(registry);

    const GraphFrameContext callerContext{ &services, &shadowView };
    graph.SetFrameContext(&callerContext);
    const SharedPhaseOutputs outputs = shared.Declare(graph, services, &shadowView, {}, settings);
    CHECK(graph.GetFrameContext() == &callerContext); // the caller's context is restored

    // Read the atlas as a graph output, standing in for a view, so the shadow pass survives.
    graph.BindOutput(graph.AddOutputSlot("atlas", RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(512, 512)),
                     outputs.Imports.at(SHADOW_ATLAS_IMPORT));

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    CHECK(cmd.DrawnIndexCounts == std::vector<uint32_t>{ 36, 36 }); // layers 0 and 5
}

TEST_CASE("The scene lights are uploaded once per frame, with or without a shadow view")
{
    const PassBuilderRegistry registry = MakeEngineRegistry();
    const HX::RenderLight     lights[2] = {};

    TestDevice         device;
    RenderGraphRuntime graph(device, ARENA_BYTES);
    FakeServices       services;
    SharedPhase        shared(registry);

    const SharedPhaseOutputs outputs = shared.Declare(graph, services, nullptr, lights, {});
    CHECK(services.SceneLightCounts == std::vector<int32_t>{ 2 });
    CHECK(outputs.SceneLights != nullptr);
    CHECK(outputs.Imports.empty()); // no camera to fit cascades to: no atlas
    CHECK(graph.GetDescription().Passes.empty());
}

TEST_CASE("The shadow view is the highest-priority view with a camera")
{
    std::vector<View> views = { MakeView(1, 5, false), MakeView(2, 1, true), MakeView(3, 3, true),
                                MakeView(4, 3, true) };
    CHECK(SelectShadowView(views) == &views[2]); // priority 3; the earlier of the tied pair

    views[2].Desc.Camera.reset();
    views[3].Desc.Camera.reset();
    CHECK(SelectShadowView(views) == &views[1]);

    views[1].Desc.Camera.reset();
    CHECK(SelectShadowView(views) == nullptr);
}
