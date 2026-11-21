#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/FxaaPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cstring>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t WIDTH  = 64;
    constexpr uint32_t HEIGHT = 32;

    // The tone-mapped image the Fxaa pass samples.
    struct LdrTexture
    {
        TestTexture Texture{ RHI::TextureDesc{ WIDTH, HEIGHT, RHI::Format::R16G16B16A16Unorm,
                                               RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
    };

    // A lone Fxaa pass sampling ldr (imported) into a colour target of its size.
    void RecordFxaa(const GraphFrameContext* context, LdrTexture& ldr, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);

        PassInvocation  fxaa("Fxaa");
        const RGTexture ldrHandle = graph.ImportTexture("ldr", RHI::Format::R16G16B16A16Unorm, true);
        graph.BindImportedTexture(ldrHandle, &ldr.Texture);
        fxaa.SetSlot("ldr", ldrHandle);
        fxaa.SetSlot("color", graph.CreateTexture({ "color", RHI::Format::R16G16B16A16Unorm,
                                                    RGSizePolicy::MakeAbsolute(WIDTH, HEIGHT),
                                                    RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        registry.Find("Fxaa")->Build(graph, fxaa);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(WIDTH, HEIGHT)),
                         fxaa.GetSlot("color"));
        REQUIRE(graph.Execute(cmd));
    }

    FxaaPushConstants Pushed(const RecordingCommandList& cmd)
    {
        REQUIRE(cmd.PushedData.size() == 1);
        REQUIRE(cmd.PushedData[0].size() == sizeof(FxaaPushConstants));
        FxaaPushConstants constants;
        std::memcpy(&constants, cmd.PushedData[0].data(), sizeof(constants));
        return constants;
    }
}

TEST_CASE("Fxaa - one pipeline, the sampled image's set, its texel size and one fullscreen triangle")
{
    GraphFrameData    frame;
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    LdrTexture           ldr;
    RecordingCommandList cmd;
    RecordFxaa(&context, ldr, cmd);

    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "push 32" });
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{ &services.GetPipeline(EnginePipeline::Fxaa) });
    CHECK(services.SampledTextures == std::vector<const RHI::IRHITexture*>{ &ldr.Texture });
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ &services.GetSampledTextureSet() });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3 });

    const FxaaPushConstants constants = Pushed(cmd);
    CHECK(constants.TexelWidth == 1.0f / WIDTH);
    CHECK(constants.TexelHeight == 1.0f / HEIGHT);
    CHECK(constants.Enabled == 1u);

    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::DontCare);
    CHECK_FALSE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.EndRenderingCount == 1);
}

TEST_CASE("Fxaa - anti-aliasing off still draws, passing the image through")
{
    GraphFrameData frame;
    frame.PostProcess.Fxaa = false;
    FakeServices         services;
    GraphFrameContext    context{ &services, &frame };
    LdrTexture           ldr;
    RecordingCommandList cmd;
    RecordFxaa(&context, ldr, cmd);
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3 });
    CHECK(Pushed(cmd).Enabled == 0u);

    CHECK(MakeFxaaPushConstants(0, 0, true).TexelWidth == 0.0f);
}

TEST_CASE("Fxaa - its slots, and nothing recorded without a frame context")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("Fxaa") != nullptr);
    CHECK(registry.Find("Fxaa")->Slots == std::vector<std::string>{ "ldr", "color" });

    LdrTexture           ldr;
    RecordingCommandList cmd;
    RecordFxaa(nullptr, ldr, cmd);
    CHECK(cmd.Commands.empty());
}
