#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    const CompiledPass& FindPass(const CompiledGraph& graph, const std::string& name)
    {
        for (const auto& pass : graph.Passes)
            if (pass.Name == name)
                return pass;
        FAIL("pass not found: " << name);
        static CompiledPass empty;
        return empty;
    }
}

TEST_CASE("A resource's first use gets a barrier from Undefined into whatever state it needs")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    builder.AddPass("Opaque", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(tex);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& opaque = FindPass(result.Graph, "Opaque");
    REQUIRE(opaque.TextureBarriers.size() == 1);
    CHECK(opaque.TextureBarriers[0].Id == tex.Id);
    CHECK(opaque.TextureBarriers[0].Before == RHI::ResourceState::Undefined);
    CHECK(opaque.TextureBarriers[0].After == RHI::ResourceState::RenderTarget);
}

TEST_CASE("A resource read in the same state it was already left in needs no second barrier")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});

    RGTexture written{};
    builder.AddPass("Write", [&](RGPassBuilder& pass)
    {
        written = pass.StorageWrite(tex);
        pass.SetSideEffect();
    });
    builder.AddPass("ReadAgain", [&](RGPassBuilder& pass)
    {
        // StorageWrite and a second StorageWrite both map to UnorderedAccess — no state change.
        pass.StorageWrite(written);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& readAgain = FindPass(result.Graph, "ReadAgain");
    CHECK(readAgain.TextureBarriers.empty());
}

TEST_CASE("Consecutive transitions on one pass, across different resources, batch into that pass's single barrier list")
{
    GraphBuilder builder;
    const RGTexture colorTex = builder.CreateTexture({});
    const RGTexture depthTex = builder.CreateTexture({});
    const RGBuffer  instanceBuffer = builder.CreateBuffer({});

    builder.AddPass("Warmup", [&](RGPassBuilder& pass)
    {
        // Puts all three resources into a state the next pass must transition *away* from.
        pass.SampleTexture(colorTex);
        pass.DepthReadOnly(depthTex);
        pass.ReadBuffer(instanceBuffer);
        pass.SetSideEffect();
    });

    builder.AddPass("Opaque", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(RGTexture{ colorTex.Id, colorTex.Version });
        pass.DepthTarget(RGTexture{ depthTex.Id, depthTex.Version });
        pass.WriteBuffer(RGBuffer{ instanceBuffer.Id, instanceBuffer.Version });
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& opaque = FindPass(result.Graph, "Opaque");
    // All three transitions land in ONE pass's barrier lists — a single Barrier() call's worth
    // of work — not three separate calls.
    REQUIRE(opaque.TextureBarriers.size() == 2);
    REQUIRE(opaque.BufferBarriers.size() == 1);

    bool sawColor = false, sawDepth = false;
    for (const auto& barrier : opaque.TextureBarriers)
    {
        if (barrier.Id == colorTex.Id)
        {
            sawColor = true;
            CHECK(barrier.Before == RHI::ResourceState::ShaderResource);
            CHECK(barrier.After == RHI::ResourceState::RenderTarget);
        }
        if (barrier.Id == depthTex.Id)
        {
            sawDepth = true;
            CHECK(barrier.Before == RHI::ResourceState::DepthRead);
            CHECK(barrier.After == RHI::ResourceState::DepthWrite);
        }
    }
    CHECK(sawColor);
    CHECK(sawDepth);

    CHECK(opaque.BufferBarriers[0].Id == instanceBuffer.Id);
    CHECK(opaque.BufferBarriers[0].Before == RHI::ResourceState::ShaderResource);
    CHECK(opaque.BufferBarriers[0].After == RHI::ResourceState::UnorderedAccess);
}

TEST_CASE("A buffer written as UnorderedAccess by one pass and again by the next gets a UAV to UAV barrier")
{
    GraphBuilder builder;
    const RGBuffer particles = builder.ImportBuffer("Particles", 1024);

    RGBuffer emitted{};
    builder.AddPass("Emit", [&](RGPassBuilder& pass)
    {
        emitted = pass.WriteBuffer(particles);
        pass.SetSideEffect();
    });
    builder.AddPass("Simulate", [&](RGPassBuilder& pass)
    {
        pass.WriteBuffer(emitted);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& emit = FindPass(result.Graph, "Emit");
    REQUIRE(emit.BufferBarriers.size() == 1);
    CHECK(emit.BufferBarriers[0].Before == RHI::ResourceState::Undefined);
    CHECK(emit.BufferBarriers[0].After == RHI::ResourceState::UnorderedAccess);

    // Without it Simulate could read particles Emit has not finished writing.
    const CompiledPass& simulate = FindPass(result.Graph, "Simulate");
    REQUIRE(simulate.BufferBarriers.size() == 1);
    CHECK(simulate.BufferBarriers[0].Id == particles.Id);
    CHECK(simulate.BufferBarriers[0].Before == RHI::ResourceState::UnorderedAccess);
    CHECK(simulate.BufferBarriers[0].After == RHI::ResourceState::UnorderedAccess);
}

TEST_CASE("A buffer written then read as indirect arguments moves from UnorderedAccess to IndirectArgument")
{
    GraphBuilder builder;
    const RGBuffer args = builder.ImportBuffer("Args", 64);

    RGBuffer built{};
    builder.AddPass("BuildArgs", [&](RGPassBuilder& pass)
    {
        built = pass.WriteBuffer(args);
        pass.SetSideEffect();
    });
    builder.AddPass("Draw", [&](RGPassBuilder& pass)
    {
        pass.ReadIndirectArgs(built);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& draw = FindPass(result.Graph, "Draw");
    REQUIRE(draw.BufferBarriers.size() == 1);
    CHECK(draw.BufferBarriers[0].Before == RHI::ResourceState::UnorderedAccess);
    CHECK(draw.BufferBarriers[0].After == RHI::ResourceState::IndirectArgument);
    CHECK(ToResourceState(RGResourceUsage::IndirectArgs) == RHI::ResourceState::IndirectArgument);
}

TEST_CASE("An imported buffer's first barrier starts from the state it was imported in")
{
    GraphBuilder builder;
    // The previous frame's draws left it as indirect arguments.
    const RGBuffer args = builder.ImportBuffer("Args", 64, false, RHI::ResourceState::IndirectArgument);
    const RGBuffer fresh = builder.ImportBuffer("Fresh", 64);

    builder.AddPass("Reset", [&](RGPassBuilder& pass)
    {
        pass.WriteBuffer(args);
        pass.WriteBuffer(fresh);
        pass.SetSideEffect();
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);

    const CompiledPass& reset = FindPass(result.Graph, "Reset");
    REQUIRE(reset.BufferBarriers.size() == 2);
    CHECK(reset.BufferBarriers[0].Id == args.Id);
    CHECK(reset.BufferBarriers[0].Before == RHI::ResourceState::IndirectArgument);
    CHECK(reset.BufferBarriers[1].Id == fresh.Id);
    CHECK(reset.BufferBarriers[1].Before == RHI::ResourceState::Undefined);

    // Imported already in the state its first use needs: no barrier.
    GraphBuilder same;
    const RGBuffer readOnly = same.ImportBuffer("State", 64, false, RHI::ResourceState::ShaderResource);
    same.AddPass("Read", [&](RGPassBuilder& pass)
    {
        pass.ReadBuffer(readOnly);
        pass.SetSideEffect();
    });
    const CompileResult sameResult = compiler.Compile(same.GetDescription());
    REQUIRE(sameResult.Success);
    CHECK(FindPass(sameResult.Graph, "Read").BufferBarriers.empty());
}

TEST_CASE("A side-effect compute pass writing only an imported buffer is kept, and transitions it once")
{
    GraphBuilder builder;
    const RGBuffer state = builder.ImportBuffer("State", 256);
    builder.AddPass("Compute", [&](RGPassBuilder& pass)
    {
        const RGBuffer written = pass.WriteBuffer(state);
        pass.WriteBuffer(written); // a second write in the same pass: still one transition
        pass.SetSideEffect();
    });
    builder.AddPass("Unread", [&](RGPassBuilder& pass) { pass.WriteBuffer(builder.ImportBuffer("Other", 4)); });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());
    REQUIRE(result.Success);
    REQUIRE(result.Graph.Passes.size() == 1);
    CHECK(result.Graph.Passes[0].Name == "Compute");
    CHECK(result.Graph.Passes[0].BufferBarriers.size() == 1);
}
