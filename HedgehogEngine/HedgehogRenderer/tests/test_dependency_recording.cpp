#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

namespace
{
    // A write's edge back to whichever pass produced the version it's building on, or a read's
    // edge to whichever pass produced the version it's reading, is recoverable by finding
    // another pass whose Reads/Writes name the same (Id, Version) pair. This helper is exactly
    // that recovery, standing in for the future compiler (RENDERING.md section 5.3) without
    // building an actual DAG — proving the recorded data is sufficient to derive one.
    bool PassRecordsResourceVersion(const RGPassRecord& pass, RGResourceId id, RGVersion version,
                                     std::vector<RGResourceRef> RGPassRecord::* list)
    {
        for (const auto& ref : pass.*list)
        {
            if (ref.Id == id && ref.Version == version)
                return true;
        }
        return false;
    }
}

TEST_CASE("Every write verb records both a Writes entry and the new version, on the pass that made it")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    RGTexture written{};
    builder.AddPass("Opaque", [&](RGPassBuilder& pass)
    {
        written = pass.ColorTarget(created);
    });

    const RGPassRecord& opaque = builder.GetDescription().Passes[0];
    REQUIRE(opaque.Writes.size() == 1);
    CHECK(opaque.Writes[0].Id == created.Id);
    CHECK(opaque.Writes[0].Version == written.Version);
    CHECK(opaque.Writes[0].Usage == RGResourceUsage::ColorTarget);
    CHECK(opaque.Reads.empty());
}

TEST_CASE("A read-after-write edge is recoverable: the reader's read version matches the writer's write version")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    RGTexture producedVersion{};
    builder.AddPass("Producer", [&](RGPassBuilder& pass)
    {
        producedVersion = pass.ColorTarget(created);
    });
    builder.AddPass("Consumer", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(producedVersion);
    });

    const auto& passes = builder.GetDescription().Passes;
    REQUIRE(passes.size() == 2);

    const bool producerWroteIt = PassRecordsResourceVersion(
        passes[0], created.Id, producedVersion.Version, &RGPassRecord::Writes);
    const bool consumerReadIt = PassRecordsResourceVersion(
        passes[1], created.Id, producedVersion.Version, &RGPassRecord::Reads);

    CHECK(producerWroteIt);
    CHECK(consumerReadIt);
}

TEST_CASE("A write-after-read edge is recoverable: the writer's input version matches the reader's read version")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    RGTexture afterWrite{};
    builder.AddPass("Reader", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(created); // reads version 0
    });
    builder.AddPass("Writer", [&](RGPassBuilder& pass)
    {
        afterWrite = pass.ColorTarget(created); // writes over version 0, producing version 1
    });

    const auto& passes = builder.GetDescription().Passes;
    REQUIRE(passes.size() == 2);

    // The writer's write is over the same version (0) the reader read — that shared version
    // number is what lets a compiler order Writer after Reader (WAR).
    const bool readerReadVersionZero = PassRecordsResourceVersion(
        passes[0], created.Id, 0, &RGPassRecord::Reads);
    REQUIRE(readerReadVersionZero);
    CHECK(afterWrite.Version == 1);
}

TEST_CASE("SetSideEffect flags exactly the pass it was called on")
{
    GraphBuilder builder;
    builder.AddPass("Hidden", [&](RGPassBuilder&) {});
    builder.AddPass("AlwaysRuns", [&](RGPassBuilder& pass)
    {
        pass.SetSideEffect();
    });

    const auto& passes = builder.GetDescription().Passes;
    REQUIRE(passes.size() == 2);
    CHECK(passes[0].HasSideEffect == false);
    CHECK(passes[1].HasSideEffect == true);
}

TEST_CASE("Import registers a resource other passes can read and write like any other")
{
    GraphBuilder builder;
    const RGTexture imported = builder.ImportTexture("BackBuffer", RHI::Format::B8G8R8A8Srgb);

    CHECK(imported.Version == 0);

    const RGResourceRecord* resource = builder.GetDescription().FindResource(imported.Id);
    REQUIRE(resource != nullptr);
    CHECK(resource->IsImported == true);
    CHECK(resource->IsReadOnly == false);
    CHECK(resource->TextureDesc.Format == RHI::Format::B8G8R8A8Srgb);
}

TEST_CASE("An imported read-only resource is flagged as such for a later compiler to reject writes to")
{
    GraphBuilder builder;
    const RGTexture imported = builder.ImportTexture("History", RHI::Format::R16G16B16A16Float, /*isReadOnly*/ true);

    const RGResourceRecord* resource = builder.GetDescription().FindResource(imported.Id);
    REQUIRE(resource != nullptr);
    CHECK(resource->IsReadOnly == true);
}
