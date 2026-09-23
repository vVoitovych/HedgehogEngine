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
