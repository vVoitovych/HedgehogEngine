#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/BloomPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t SIZE = 128;

    struct HdrTexture
    {
        TestTexture Texture{ RHI::TextureDesc{ SIZE, SIZE, RHI::Format::R16G16B16A16Float,
                                               RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
    };

    // A lone Bloom pass sampling hdr (imported) into a bloom target of half its size.
    void RecordBloom(const GraphFrameContext* context, HdrTexture& hdr, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);
        graph.SetSizeReferences(SIZE, SIZE, SIZE, SIZE);

        PassInvocation  bloom("Bloom");
        const RGTexture hdrHandle = graph.ImportTexture("hdr", RHI::Format::R16G16B16A16Float, true);
        graph.BindImportedTexture(hdrHandle, &hdr.Texture);
        bloom.SetSlot("hdr", hdrHandle);
        bloom.SetSlot("bloom", graph.CreateTexture({ "bloom", RHI::Format::R16G16B16A16Float,
                                                     RGSizePolicy::MakeRelativeToResult(0.5f),
                                                     RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        registry.Find("Bloom")->Build(graph, bloom);
        graph.BindOutput(graph.AddOutputSlot("bloom", RHI::Format::R16G16B16A16Float, RGSizePolicy::MakeRelativeToResult(0.5f)),
                         bloom.GetSlot("bloom"));
        REQUIRE(graph.Execute(cmd));
    }
}

TEST_CASE("Bloom - on, five downsamples and five upsamples, each with its level's set and texel size")
{
    GraphFrameData frame;
    frame.PostProcess.Bloom          = true;
    frame.PostProcess.BloomThreshold = 2.0f;
    frame.PostProcess.BloomKnee      = 0.5f;
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    HdrTexture           hdr;
    RecordingCommandList cmd;
    RecordBloom(&context, hdr, cmd);

    std::vector<std::string> expected;
    for (int i = 0; i < 10; ++i)
        expected.insert(expected.end(), { "begin", "pipeline", "set 0", "push 32" });
    CHECK(cmd.Commands == expected);
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>(10, 3));
    REQUIRE(cmd.BoundPipelines.size() == 10);
    for (size_t i = 0; i < 10; ++i)
    {
        CAPTURE(i);
        CHECK(cmd.BoundPipelines[i] == &services.GetPipeline(i < 5 ? EnginePipeline::BloomDownsample
                                                                   : EnginePipeline::BloomUpsample));
    }

    // Down: from the HDR target into 1/2, 1/4 ... 1/32 of the view; up: back onto 1/16 ... 1/2,
    // loading what is there, then into bloom, cleared.
    REQUIRE(cmd.Renderings.size() == 10);
    const uint32_t sizes[10] = { 64, 32, 16, 8, 4, 8, 16, 32, 64, 64 };
    for (size_t i = 0; i < 10; ++i)
    {
        CAPTURE(i);
        CHECK(cmd.Renderings[i].Width == sizes[i]);
        const RHI::LoadOp load = cmd.Renderings[i].ColorAttachments[0].LoadOp;
        CHECK(load == (i < 5 ? RHI::LoadOp::DontCare : i < 9 ? RHI::LoadOp::Load : RHI::LoadOp::Clear));
    }
    REQUIRE(services.SampledTextures.size() == 10);
    CHECK(services.SampledTextures[0] == &hdr.Texture);
    for (size_t i = 1; i < 5; ++i)
        CHECK(services.SampledTextures[i] == cmd.Renderings[i - 1].ColorAttachments[0].Texture);

    REQUIRE(cmd.PushedData.size() == 10);
    BloomDownsamplePushConstants first;
    std::memcpy(&first, cmd.PushedData[0].data(), sizeof(first));
    CHECK(first.Prefilter == 1u);
    CHECK(first.TexelWidth == 1.0f / SIZE);
    CHECK(first.Threshold == 2.0f);
    CHECK(first.Knee == 1.0f);
    BloomDownsamplePushConstants second;
    std::memcpy(&second, cmd.PushedData[1].data(), sizeof(second));
    CHECK(second.Prefilter == 0u);
    CHECK(second.TexelWidth == 1.0f / 64);
    BloomUpsamplePushConstants up;
    std::memcpy(&up, cmd.PushedData[5].data(), sizeof(up));
    CHECK(up.TexelWidth == 1.0f / 4);
}

TEST_CASE("Bloom - off, only bloom is cleared to black; nothing without a frame context")
{
    GraphFrameData       frame; // bloom off by default
    FakeServices         services;
    GraphFrameContext    context{ &services, &frame };
    HdrTexture           hdr;
    RecordingCommandList cmd;
    RecordBloom(&context, hdr, cmd);
    CHECK(cmd.Commands == std::vector<std::string>{ "begin" });
    CHECK(cmd.DrawnVertexCounts.empty());
    REQUIRE(cmd.Renderings.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Clear);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Clear.Color.R == 0.0f);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Clear.Color.A == 0.0f);

    RecordingCommandList none;
    RecordBloom(nullptr, hdr, none);
    CHECK(none.Commands.empty());

    PostProcessSettings bad;
    bad.BloomThreshold = std::numeric_limits<float>::quiet_NaN();
    bad.BloomKnee      = -1.0f;
    const BloomDownsamplePushConstants fallback = MakeBloomDownsamplePushConstants(0, 0, bad, true);
    CHECK(fallback.Threshold == 1.0f);
    CHECK(fallback.Knee == 0.5f);
    CHECK(fallback.TexelWidth == 0.0f);
}
