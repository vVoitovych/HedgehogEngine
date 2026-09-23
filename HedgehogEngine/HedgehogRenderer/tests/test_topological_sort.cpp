#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    size_t IndexOfPass(const CompiledGraph& graph, const std::string& name)
    {
        for (size_t i = 0; i < graph.Passes.size(); ++i)
            if (graph.Passes[i].Name == name)
                return i;
        FAIL("pass not found: " << name);
        return static_cast<size_t>(-1);
    }
}

TEST_CASE("A read-after-write dependency orders the producer before its reader")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    RGTexture produced{};
    builder.AddPass("Producer", [&](RGPassBuilder& pass)
    {
        produced = pass.ColorTarget(tex);
    });
    builder.AddPass("Consumer", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(produced);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    CHECK(IndexOfPass(result.Graph, "Producer") < IndexOfPass(result.Graph, "Consumer"));
}

TEST_CASE("A write-after-read hazard orders every reader of the old version before the overwriter")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    builder.AddPass("Reader", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(tex); // reads version 0
        pass.SetSideEffect();
    });
    builder.AddPass("Overwriter", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(tex); // overwrites version 0 -> 1; must not run before Reader
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    CHECK(IndexOfPass(result.Graph, "Reader") < IndexOfPass(result.Graph, "Overwriter"));
}

TEST_CASE("A diamond dependency (two independent middle passes) keeps both orderings valid but respects the ends")
{
    GraphBuilder builder;
    const RGTexture source = builder.CreateTexture({});
    const RGTexture left   = builder.CreateTexture({});
    const RGTexture right  = builder.CreateTexture({});
    const RGTexture sink   = builder.CreateTexture({});

    RGTexture sourceAfter{};
    builder.AddPass("Source", [&](RGPassBuilder& pass)
    {
        sourceAfter = pass.ColorTarget(source);
    });

    RGTexture leftAfter{};
    builder.AddPass("Left", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(sourceAfter);
        leftAfter = pass.ColorTarget(left);
    });

    RGTexture rightAfter{};
    builder.AddPass("Right", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(sourceAfter);
        rightAfter = pass.ColorTarget(right);
    });

    builder.AddPass("Sink", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(leftAfter);
        pass.SampleTexture(rightAfter);
        pass.ColorTarget(sink);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    REQUIRE(result.Graph.Passes.size() == 4);

    const size_t sourceIdx = IndexOfPass(result.Graph, "Source");
    const size_t leftIdx   = IndexOfPass(result.Graph, "Left");
    const size_t rightIdx  = IndexOfPass(result.Graph, "Right");
    const size_t sinkIdx   = IndexOfPass(result.Graph, "Sink");

    CHECK(sourceIdx < leftIdx);
    CHECK(sourceIdx < rightIdx);
    CHECK(leftIdx < sinkIdx);
    CHECK(rightIdx < sinkIdx);
}
