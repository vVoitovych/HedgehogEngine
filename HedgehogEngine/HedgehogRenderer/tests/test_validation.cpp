#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    bool AnyErrorMentions(const std::vector<CompileError>& errors, const std::string& text)
    {
        for (const auto& error : errors)
            if (error.Message.find(text) != std::string::npos)
                return true;
        return false;
    }
}

TEST_CASE("Reading a version with no producer fails, naming the pass and the resource")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    builder.AddPass("Ghost", [&](RGPassBuilder& pass)
    {
        // Version 1 was never produced by anyone — nothing ever called a write verb on `created`.
        pass.SampleTexture(RGTexture{ created.Id, created.Version + 1 });
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].PassName == "Ghost");
    CHECK(AnyErrorMentions(result.Errors, "Ghost"));
    CHECK(AnyErrorMentions(result.Errors, "no pass produced"));
}

TEST_CASE("Writing an imported read-only resource fails, naming the pass and the resource")
{
    GraphBuilder builder;
    const RGTexture history = builder.ImportTexture("History", RHI::Format::R16G16B16A16Float, /*isReadOnly*/ true);

    builder.AddPass("Overwriter", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(history);
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].PassName == "Overwriter");
    CHECK(result.Errors[0].ResourceName == "History");
    CHECK(AnyErrorMentions(result.Errors, "read-only"));
}

TEST_CASE("An unbound output slot fails, naming the slot")
{
    GraphBuilder builder;
    builder.AddOutputSlot("color", RHI::Format::R16G16B16A16Float, {});
    // Never called BindOutput.

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(result.Errors[0].ResourceName == "color");
    CHECK(AnyErrorMentions(result.Errors, "never bound"));
}

TEST_CASE("A cycle between two passes is detected and reported")
{
    GraphBuilder builder;
    const RGTexture a = builder.CreateTexture({});
    const RGTexture b = builder.CreateTexture({});

    RGTexture aAfter{};
    RGTexture bAfter{};

    // PassOne reads b's version 1, which only PassTwo can produce; PassTwo reads a's version 1,
    // which only PassOne can produce. Neither can run first — a genuine cycle.
    builder.AddPass("PassOne", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(RGTexture{ b.Id, 1 });
        aAfter = pass.ColorTarget(a);
        pass.SetSideEffect();
    });
    builder.AddPass("PassTwo", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(RGTexture{ a.Id, 1 });
        bAfter = pass.ColorTarget(b);
        pass.SetSideEffect();
    });

    (void)aAfter;
    (void)bAfter;

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    // Both passes DO have producers for what they read (each other), so static validation
    // passes; the cycle only surfaces once the topological sort can't fully drain the ready
    // queue.
    REQUIRE_FALSE(result.Success);
    REQUIRE(result.Errors.size() == 1);
    CHECK(AnyErrorMentions(result.Errors, "PassOne"));
    CHECK(AnyErrorMentions(result.Errors, "PassTwo"));
    CHECK(AnyErrorMentions(result.Errors, "Cycle"));
}

TEST_CASE("Multiple independent problems are all reported from a single Compile() call")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});
    const RGTexture history = builder.ImportTexture("History", RHI::Format::D32Float, /*isReadOnly*/ true);
    builder.AddOutputSlot("unbound", RHI::Format::R8G8B8A8Unorm, {});

    builder.AddPass("BadReader", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(RGTexture{ created.Id, created.Version + 1 });
    });
    builder.AddPass("BadWriter", [&](RGPassBuilder& pass)
    {
        pass.ColorTarget(history);
    });

    const GraphCompiler compiler;
    const CompileResult result = compiler.Compile(builder.GetDescription());

    REQUIRE_FALSE(result.Success);
    CHECK(result.Errors.size() == 3); // BadReader, BadWriter, unbound slot
}
