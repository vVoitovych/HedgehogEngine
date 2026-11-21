#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/SsaoPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t SIZE = 64;

    struct DepthTexture
    {
        TestTexture Texture{ RHI::TextureDesc{ SIZE, SIZE, RHI::Format::D32Float,
                                               RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled } };
    };

    // A lone Ssao pass sampling depth (imported) into an ao target of half its size.
    void RecordSsao(const GraphFrameContext* context, DepthTexture& depth, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);
        graph.SetSizeReferences(SIZE, SIZE, SIZE, SIZE);

        PassInvocation  ssao("Ssao");
        const RGTexture depthHandle = graph.ImportTexture("depth", RHI::Format::D32Float, true);
        graph.BindImportedTexture(depthHandle, &depth.Texture);
        ssao.SetSlot("depth", depthHandle);
        ssao.SetSlot("ao", graph.CreateTexture({ "ao", RHI::Format::R8Unorm, RGSizePolicy::MakeRelativeToResult(0.5f),
                                                 RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        registry.Find("Ssao")->Build(graph, ssao);
        graph.BindOutput(graph.AddOutputSlot("ao", RHI::Format::R8Unorm, RGSizePolicy::MakeRelativeToResult(0.5f)),
                         ssao.GetSlot("ao"));
        REQUIRE(graph.Execute(cmd));
    }

    template <typename T>
    T Pushed(const RecordingCommandList& cmd, size_t index)
    {
        REQUIRE(cmd.PushedData.size() > index);
        REQUIRE(cmd.PushedData[index].size() == sizeof(T));
        T constants;
        std::memcpy(&constants, cmd.PushedData[index].data(), sizeof(constants));
        return constants;
    }
}

TEST_CASE("Ssao - on, the occlusion from the depth into its own target, then the blur into ao")
{
    GraphFrameData frame;
    frame.Proj                   = HM::Matrix4x4::Perspective(1.0f, 1.0f, 0.1f, 100.0f);
    frame.PostProcess.Ssao       = true;
    frame.PostProcess.SsaoRadius = 0.75f;
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    DepthTexture         depth;
    RecordingCommandList cmd;
    RecordSsao(&context, depth, cmd);

    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "push 64",                // occlusion
                                                    "begin", "pipeline", "set 0", "set 1", "push 32" });   // blur
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{ &services.GetPipeline(EnginePipeline::Ssao),
                                                                       &services.GetPipeline(EnginePipeline::SsaoBlur) });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3, 3 });
    // The depth for the occlusion, then the raw occlusion and the depth for the blur.
    REQUIRE(services.SampledTextures.size() == 3);
    CHECK(services.SampledTextures[0] == &depth.Texture);
    CHECK(services.SampledTextures[2] == &depth.Texture);
    REQUIRE(cmd.Renderings.size() == 2);
    CHECK(services.SampledTextures[1] == cmd.Renderings[0].ColorAttachments[0].Texture);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Texture != cmd.Renderings[1].ColorAttachments[0].Texture);
    CHECK(cmd.Renderings[1].Width == SIZE / 2);
    CHECK_FALSE(cmd.Renderings[1].DepthAttachment.has_value());

    const SsaoPushConstants occlusion = Pushed<SsaoPushConstants>(cmd, 0);
    CHECK(occlusion.Radius == 0.75f);
    CHECK(occlusion.TexelWidth == 1.0f / SIZE);
    CHECK(occlusion.Projection.E == -1.0f);
    CHECK(Pushed<SsaoBlurPushConstants>(cmd, 1).TexelWidth == 1.0f / (SIZE / 2));
}

TEST_CASE("Ssao - off, ao is only cleared to white; nothing without a frame context")
{
    GraphFrameData       frame; // SSAO off by default
    FakeServices         services;
    GraphFrameContext    context{ &services, &frame };
    DepthTexture         depth;
    RecordingCommandList cmd;
    RecordSsao(&context, depth, cmd);
    CHECK(cmd.Commands == std::vector<std::string>{ "begin" });
    CHECK(cmd.DrawnVertexCounts.empty());
    REQUIRE(cmd.Renderings.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Clear);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Clear.Color.R == 1.0f);

    RecordingCommandList none;
    RecordSsao(nullptr, depth, none);
    CHECK(none.Commands.empty());
}

TEST_CASE("Ssao - its push constants")
{
    // A perspective projection's view depth comes back from its clip terms: the near plane at 0, the far at 1.
    const HM::Matrix4x4  proj = HM::Matrix4x4::Perspective(1.0f, 1.5f, 0.5f, 50.0f);
    const SsaoProjection p    = MakeSsaoProjection(proj);
    const auto viewZ = [&](float depth) { return (p.D - depth * p.F) / (depth * p.E - p.C); };
    CHECK(viewZ(0.0f) == doctest::Approx(-0.5f));
    CHECK(viewZ(1.0f) == doctest::Approx(-50.0f));
    CHECK(p.A == proj[0][0]);
    CHECK(p.B == proj[1][1]);

    PostProcessSettings settings;
    settings.SsaoRadius    = -1.0f;
    settings.SsaoIntensity = std::numeric_limits<float>::quiet_NaN();
    const SsaoPushConstants constants = MakeSsaoPushConstants(proj, settings, 0, 0);
    CHECK(constants.Radius == 0.0f);
    CHECK(constants.Intensity == 1.0f);
    CHECK(constants.TexelWidth == 0.0f);
    CHECK(MakeSsaoBlurPushConstants(proj, 8, 4).TexelHeight == 0.25f);

    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("Ssao") != nullptr);
    CHECK(registry.Find("Ssao")->Slots == std::vector<std::string>{ "depth", "ao" });
    CHECK(registry.Find("Forward")->OptionalSlots == std::vector<std::string>{ "ao" });
}
