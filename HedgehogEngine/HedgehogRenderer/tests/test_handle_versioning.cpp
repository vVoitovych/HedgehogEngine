#include "HedgehogRenderer/Graph/GraphBuilder.hpp"
#include "HedgehogRenderer/Graph/RGPassBuilder.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;

TEST_CASE("CreateTexture/CreateBuffer return distinct ids starting at version 0")
{
    GraphBuilder builder;

    RGTextureDesc colorDesc;
    colorDesc.Name = "SceneColor";
    const RGTexture color = builder.CreateTexture(colorDesc);

    RGBufferDesc bufferDesc;
    bufferDesc.Name = "Instances";
    const RGBuffer instances = builder.CreateBuffer(bufferDesc);

    CHECK(color.Version == 0);
    CHECK(instances.Version == 0);
    CHECK(color.Id != INVALID_RG_RESOURCE_ID);
    CHECK(instances.Id != INVALID_RG_RESOURCE_ID);
    CHECK(color.Id != instances.Id);
}

TEST_CASE("Every write verb returns a new, never-before-seen version")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    RGTexture afterFirstWrite{};
    RGTexture afterSecondWrite{};

    builder.AddPass("Opaque", [&](RGPassBuilder& pass)
    {
        afterFirstWrite = pass.ColorTarget(created);
    });
    builder.AddPass("Transparent", [&](RGPassBuilder& pass)
    {
        afterSecondWrite = pass.ColorTarget(afterFirstWrite);
    });

    CHECK(afterFirstWrite.Id == created.Id);
    CHECK(afterFirstWrite.Version == created.Version + 1);
    CHECK(afterSecondWrite.Id == created.Id);
    CHECK(afterSecondWrite.Version == afterFirstWrite.Version + 1);
    CHECK(afterSecondWrite.Version != created.Version);
    CHECK(afterSecondWrite.Version != afterFirstWrite.Version);
}

TEST_CASE("Different write verbs (color, depth, storage, buffer) each bump their own resource independently")
{
    GraphBuilder builder;
    const RGTexture colorTex = builder.CreateTexture({});
    const RGTexture depthTex = builder.CreateTexture({});
    const RGBuffer  buffer   = builder.CreateBuffer({});

    RGTexture colorAfter{};
    RGTexture depthAfter{};
    RGBuffer  bufferAfter{};

    builder.AddPass("Pass", [&](RGPassBuilder& pass)
    {
        colorAfter  = pass.ColorTarget(colorTex);
        depthAfter  = pass.DepthTarget(depthTex);
        bufferAfter = pass.WriteBuffer(buffer);
    });

    CHECK(colorAfter.Version == 1);
    CHECK(depthAfter.Version == 1);
    CHECK(bufferAfter.Version == 1);
    // Bumping one resource's version must not disturb another's.
    CHECK(colorAfter.Id != depthAfter.Id);
    CHECK(colorAfter.Id != bufferAfter.Id);
}

TEST_CASE("StorageWrite is overloaded for both textures and buffers")
{
    GraphBuilder builder;
    const RGTexture tex = builder.CreateTexture({});
    const RGBuffer  buf = builder.CreateBuffer({});

    RGTexture texAfter{};
    RGBuffer  bufAfter{};
    builder.AddPass("Compute", [&](RGPassBuilder& pass)
    {
        texAfter = pass.StorageWrite(tex);
        bufAfter = pass.StorageWrite(buf);
    });

    CHECK(texAfter.Version == 1);
    CHECK(bufAfter.Version == 1);
}

TEST_CASE("Reading a texture does not change its version")
{
    GraphBuilder builder;
    const RGTexture created = builder.CreateTexture({});

    builder.AddPass("Read", [&](RGPassBuilder& pass)
    {
        pass.SampleTexture(created);
    });

    const GraphDescription& description = builder.GetDescription();
    const RGResourceRecord* resource = description.FindResource(created.Id);
    REQUIRE(resource != nullptr);
    CHECK(resource->LatestVersion == 0);
}
