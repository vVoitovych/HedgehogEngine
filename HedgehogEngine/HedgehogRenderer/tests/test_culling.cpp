#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    bool HasPassNamed(const CompiledGraph& graph, const std::string& name)
    {
        for (const auto& pass : graph.Passes)
            if (pass.Name == name)
                return true;
        return false;
    }
}

TEST_CASE("A pass whose output is never read and has no side effect is culled")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    builder.AddPass("Dead", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(tex); // nobody reads this, no side effect, no output slot binds it
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    CHECK(result.Graph.Passes.empty());
    CHECK_FALSE(HasPassNamed(result.Graph, "Dead"));
}

TEST_CASE("A SetSideEffect pass is never culled, even with no readers")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    builder.AddPass("AlwaysRuns", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(tex);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    REQUIRE(result.Graph.Passes.size() == 1);
    CHECK(result.Graph.Passes[0].Name == "AlwaysRuns");
}

TEST_CASE("A pass whose output feeds a bound output slot is kept, and its own producer chain with it")
{
    GraphBuilder clean;
    const RGTexture sceneColor = clean.CreateTexture({});
    const RGTexture bloomTex   = clean.CreateTexture({});

    RGTexture sceneAfter{};
    clean.AddPass("Opaque", [&](RGPassBuilder& pass)
    {
        sceneAfter = pass.ColorTarget(sceneColor);
    });
    RGTexture bloomAfter{};
    clean.AddPass("Bloom", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(sceneAfter);
        bloomAfter = pass.ColorTarget(bloomTex);
    });
    const uint32_t slot = clean.AddOutputSlot("color", RHI::Format::R16G16B16A16Float, {});
    clean.BindOutput(slot, bloomAfter);

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(clean.GetDescription());

    REQUIRE(result.Success);
    // Both Opaque (feeds Bloom's read) and Bloom (feeds the bound slot) must survive culling.
    CHECK(HasPassNamed(result.Graph, "Opaque"));
    CHECK(HasPassNamed(result.Graph, "Bloom"));
    CHECK(result.Graph.Passes.size() == 2);
}

TEST_CASE("An upstream pass whose only reader is culled is itself culled transitively")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    RGTexture afterWrite{};
    builder.AddPass("Upstream", [&](RGPassBuilder& pass)
    {
        afterWrite = pass.ColorTarget(tex);
    });
    builder.AddPass("DeadReader", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(afterWrite); // reads it, but DeadReader itself is never used further
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE(result.Success);
    CHECK(result.Graph.Passes.empty());
}
