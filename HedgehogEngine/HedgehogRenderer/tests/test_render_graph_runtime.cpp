#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

using namespace Renderer;
using namespace RGTest;

namespace
{
    struct OpaquePassData
    {
        RGTexture ColorTarget{};
    };

    RHI::TextureDesc MakeDesc(uint32_t width, uint32_t height)
    {
        RHI::TextureDesc desc;
        desc.Width  = width;
        desc.Height = height;
        desc.Format = RHI::Format::R8G8B8A8Unorm;
        desc.Usage  = RHI::TextureUsage::ColorAttachment;
        return desc;
    }

    RGTextureDesc MakeRGDesc(uint32_t width, uint32_t height)
    {
        RGTextureDesc desc;
        desc.Name   = "SceneColor";
        desc.Format = RHI::Format::R8G8B8A8Unorm;
        desc.Size   = RGSizePolicy::MakeAbsolute(width, height);
        desc.Usage  = RHI::TextureUsage::ColorAttachment;
        return desc;
    }
}

TEST_CASE("AddPass's PassData and execute closure both live inside the runtime's frame arena")
{
    TestDevice device;
    RenderGraphRuntime runtime(device, 64 * 1024);

    bool passDataInArena = false;
    bool executeRan      = false;

    const RGTexture tex = runtime.CreateTexture(MakeRGDesc(64, 64));

    runtime.AddPass<OpaquePassData>(
        "Opaque",
        [&](RGPassBuilder& pass, OpaquePassData& data)
        {
            passDataInArena = runtime.GetArena().Owns(&data);
            data.ColorTarget = pass.ColorTarget(tex);
            pass.SetSideEffect();
        },
        [&](OpaquePassData&, RHI::IRHICommandList&)
        {
            executeRan = true;
        });

    RecordingCommandList cmdList;
    const bool success = runtime.Execute(cmdList);

    REQUIRE(success);
    CHECK(passDataInArena);
    CHECK(executeRan);
}

TEST_CASE("Execute issues a pass's barriers before invoking its closure")
{
    TestDevice device;
    RenderGraphRuntime runtime(device, 64 * 1024);

    const RGTexture tex = runtime.CreateTexture(MakeRGDesc(64, 64));

    bool barriersSeenBeforeExecute = false;

    RecordingCommandList cmdList;

    runtime.AddPass<OpaquePassData>(
        "Opaque",
        [&](RGPassBuilder& pass, OpaquePassData& data)
        {
            data.ColorTarget = pass.ColorTarget(tex);
            pass.SetSideEffect();
        },
        [&](OpaquePassData&, RHI::IRHICommandList&)
        {
            // By the time this runs, Execute must already have issued this pass's barrier.
            barriersSeenBeforeExecute = !cmdList.Calls.empty();
        });

    const bool success = runtime.Execute(cmdList);

    REQUIRE(success);
    CHECK(barriersSeenBeforeExecute);
    REQUIRE(cmdList.Calls.size() == 1);
    REQUIRE(cmdList.Calls[0].TextureBarriers.size() == 1);
    CHECK(cmdList.Calls[0].TextureBarriers[0].Before == RHI::ResourceState::Undefined);
    CHECK(cmdList.Calls[0].TextureBarriers[0].After == RHI::ResourceState::RenderTarget);
}

TEST_CASE("A culled pass's execute closure never runs")
{
    TestDevice device;
    RenderGraphRuntime runtime(device, 64 * 1024);

    const RGTexture tex = runtime.CreateTexture(MakeRGDesc(64, 64));

    bool executeRan = false;
    runtime.AddPass<OpaquePassData>(
        "Dead",
        [&](RGPassBuilder& pass, OpaquePassData& data)
        {
            data.ColorTarget = pass.ColorTarget(tex); // nobody reads it, no side effect
        },
        [&](OpaquePassData&, RHI::IRHICommandList&)
        {
            executeRan = true;
        });

    RecordingCommandList cmdList;
    const bool success = runtime.Execute(cmdList);

    REQUIRE(success);
    CHECK_FALSE(executeRan);
    CHECK(cmdList.Calls.empty());
}

TEST_CASE("A Compile() failure is reported and runs no passes, but still resets for the next frame")
{
    TestDevice device;
    RenderGraphRuntime runtime(device, 64 * 1024);

    runtime.AddOutputSlot("color", RHI::Format::R8G8B8A8Unorm, {}); // never bound -> compile failure

    bool executeRan = false;
    runtime.AddPass<OpaquePassData>(
        "Opaque",
        [&](RGPassBuilder& pass, OpaquePassData&) { pass.SetSideEffect(); },
        [&](OpaquePassData&, RHI::IRHICommandList&) { executeRan = true; });

    RecordingCommandList cmdList;
    const bool success = runtime.Execute(cmdList);

    CHECK_FALSE(success);
    CHECK_FALSE(executeRan);

    // The next frame's declarations must start from a clean graph, not the failed one.
    const RGTexture freshTex = runtime.CreateTexture(MakeRGDesc(32, 32));
    bool secondFrameRan = false;
    runtime.AddPass<OpaquePassData>(
        "SecondFrame",
        [&](RGPassBuilder& pass, OpaquePassData& data)
        {
            data.ColorTarget = pass.ColorTarget(freshTex);
            pass.SetSideEffect();
        },
        [&](OpaquePassData&, RHI::IRHICommandList&) { secondFrameRan = true; });

    RecordingCommandList secondCmdList;
    CHECK(runtime.Execute(secondCmdList));
    CHECK(secondFrameRan);
}

TEST_CASE("An imported texture keeps its exact identity across Execute — the pool never touches it")
{
    TestDevice device;
    RenderGraphRuntime runtime(device, 64 * 1024);

    RHI::TextureDesc desc = MakeDesc(128, 128);
    auto persistentStorage = std::make_unique<TestTexture>(desc);
    RHI::IRHITexture* persistent = persistentStorage.get();

    const RGTexture imported = runtime.ImportTexture("History", RHI::Format::R8G8B8A8Unorm);
    runtime.BindImportedTexture(imported, persistent);

    RHI::IRHITexture* seenDuringExecute = nullptr;
    runtime.AddPass<OpaquePassData>(
        "Sample",
        [&](RGPassBuilder& pass, OpaquePassData&)
        {
            pass.SampleTexture(imported);
            pass.SetSideEffect();
        },
        [&](OpaquePassData&, RHI::IRHICommandList&) {});

    RecordingCommandList cmdList;
    REQUIRE(runtime.Execute(cmdList));

    REQUIRE(cmdList.Calls.size() == 1);
    REQUIRE(cmdList.Calls[0].TextureBarriers.size() == 1);
    seenDuringExecute = cmdList.Calls[0].TextureBarriers[0].Texture;
    CHECK(seenDuringExecute == persistent);
    CHECK(device.m_TexturesCreated == 0); // the pool was never asked to create anything for it
}

TEST_CASE("Relative sizes resolve against the references set for the view being declared")
{
    TestDevice device;
    RenderGraphRuntime graph(device, 16 * 1024);

    RGTextureDesc half = MakeRGDesc(0, 0);
    half.Size = RGSizePolicy::MakeRelativeToResult(0.5f);
    RGTextureDesc window = MakeRGDesc(0, 0);
    window.Name = "Window";
    window.Size = RGSizePolicy::MakeRelativeToSwapchain(1.0f);

    const auto sizeOf = [&](RGTexture texture)
    {
        return graph.GetDescription().FindResource(texture.Id)->TextureDesc.Size;
    };

    // No reference yet: the policy stays relative, as in the headless oracle tests.
    CHECK(sizeOf(graph.CreateTexture(half)).Kind == RGSizePolicyKind::RelativeToResult);

    graph.SetSizeReferences(640, 360, 1920, 1080);
    const RGSizePolicy halfSize   = sizeOf(graph.CreateTexture(half));
    const RGSizePolicy windowSize = sizeOf(graph.CreateTexture(window));
    CHECK(halfSize.Kind == RGSizePolicyKind::Absolute);
    CHECK(halfSize.Width == 320);
    CHECK(halfSize.Height == 180);
    CHECK(windowSize.Width == 1920);
    CHECK(windowSize.Height == 1080);

    // Execute clears the references: the next frame sets them again for each view.
    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    CHECK(sizeOf(graph.CreateTexture(half)).Kind == RGSizePolicyKind::RelativeToResult);
}
