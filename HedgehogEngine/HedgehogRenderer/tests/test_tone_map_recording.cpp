#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/ToneMapPass.hpp"

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
    constexpr uint32_t TARGET_SIZE = 64;

    // What a view's forward pass renders into: the texture the ToneMap pass samples.
    struct HdrTexture
    {
        TestTexture Texture{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::R16G16B16A16Float,
                                               RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
    };

    // A lone ToneMap pass sampling hdr (imported) into a TARGET_SIZE colour target, run with context
    // (none, as headless graphs run) into cmd.
    void RecordToneMap(const GraphFrameContext* context, HdrTexture& hdr, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);

        PassInvocation  toneMap("ToneMap");
        const RGTexture hdrHandle = graph.ImportTexture("hdr", RHI::Format::R16G16B16A16Float, true);
        graph.BindImportedTexture(hdrHandle, &hdr.Texture);
        toneMap.SetSlot("hdr", hdrHandle);
        toneMap.SetSlot("color", graph.CreateTexture({ "color", RHI::Format::R16G16B16A16Unorm,
                                                       RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE),
                                                       RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        registry.Find("ToneMap")->Build(graph, toneMap);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm,
                                             RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE)),
                         toneMap.GetSlot("color"));

        REQUIRE(graph.Execute(cmd));
    }

    float PushedExposureScale(const RecordingCommandList& cmd)
    {
        REQUIRE(cmd.PushedData.size() == 1);
        REQUIRE(cmd.PushedData[0].size() == sizeof(ToneMapPushConstants));
        ToneMapPushConstants constants;
        std::memcpy(&constants, cmd.PushedData[0].data(), sizeof(constants));
        return constants.ExposureScale;
    }
}

TEST_CASE("ToneMap - one pipeline, the sampled HDR set, the exposure and one fullscreen triangle")
{
    GraphFrameData frame;
    frame.Exposure = 1.0f;
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    HdrTexture           hdr;
    RecordingCommandList cmd;
    RecordToneMap(&context, hdr, cmd);

    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "push 16" });
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{ &services.GetPipeline(EnginePipeline::ToneMap) });
    CHECK(services.SampledTextures == std::vector<const RHI::IRHITexture*>{ &hdr.Texture });
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ &services.GetSampledTextureSet() });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3 });
    CHECK(cmd.DrawnIndexCounts.empty());
    CHECK(PushedExposureScale(cmd) == 2.0f);

    // Every pixel of the colour is written: nothing to load, no depth.
    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::DontCare);
    CHECK_FALSE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.Renderings[0].Width == TARGET_SIZE);
    CHECK(cmd.EndRenderingCount == 1);
}

TEST_CASE("ToneMap - an exposure of +1 EV doubles the radiance before the curve, and 0 EV leaves it")
{
    CHECK(MakeToneMapPushConstants(0.0f).ExposureScale == 1.0f);
    CHECK(MakeToneMapPushConstants(1.0f).ExposureScale == 2.0f);
    CHECK(MakeToneMapPushConstants(-2.0f).ExposureScale == 0.25f);
    CHECK(MakeToneMapPushConstants(1.5f).ExposureScale
          == doctest::Approx(2.0f * MakeToneMapPushConstants(0.5f).ExposureScale));

    // A non-finite exposure is 0 EV.
    CHECK(MakeToneMapPushConstants(std::numeric_limits<float>::quiet_NaN()).ExposureScale == 1.0f);
    CHECK(MakeToneMapPushConstants(std::numeric_limits<float>::infinity()).ExposureScale == 1.0f);

    // A frame with no environment has 0 EV.
    const GraphFrameData noEnvironment;
    FakeServices         services;
    GraphFrameContext    context{ &services, &noEnvironment };
    HdrTexture           hdr;
    RecordingCommandList cmd;
    RecordToneMap(&context, hdr, cmd);
    CHECK(PushedExposureScale(cmd) == 1.0f);
}

TEST_CASE("ToneMap - its slots, and nothing recorded without a frame context")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("ToneMap") != nullptr);
    CHECK(registry.Find("ToneMap")->Slots == std::vector<std::string>{ "hdr", "color" });

    HdrTexture           hdr;
    RecordingCommandList cmd;
    RecordToneMap(nullptr, hdr, cmd);
    CHECK(cmd.Commands.empty());
    CHECK(cmd.DrawnVertexCounts.empty());
}
