#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/SkyboxPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t TARGET_SIZE = 64;

    // A view's HDR target and the depth its prepass wrote, which the Skybox pass draws against.
    struct ViewTargets
    {
        TestTexture Hdr{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::R16G16B16A16Float,
                                           RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
        TestTexture Depth{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::D32Float,
                                             RHI::TextureUsage::DepthStencil } };
        TestTexture Cube{ RHI::TextureDesc{ 16, 16, RHI::Format::R16G16B16A16Float, RHI::TextureUsage::Sampled,
                                            RHI::TextureType::TextureCube, 5, 6 } };
    };

    // A lone Skybox pass over the view's targets (imported, the depth read-only), run with context into cmd.
    void RecordSkybox(const GraphFrameContext* context, ViewTargets& targets, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);

        const RGTexture hdr   = graph.ImportTexture("hdr", RHI::Format::R16G16B16A16Float, false);
        const RGTexture depth = graph.ImportTexture("depth", RHI::Format::D32Float, true);
        graph.BindImportedTexture(hdr, &targets.Hdr);
        graph.BindImportedTexture(depth, &targets.Depth);

        PassInvocation skybox("Skybox");
        skybox.SetSlot("color", hdr);
        skybox.SetSlot("depth", depth);
        registry.Find("Skybox")->Build(graph, skybox);
        graph.BindOutput(graph.AddOutputSlot("hdr", RHI::Format::R16G16B16A16Float,
                                             RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE)),
                         skybox.GetSlot("color"));

        REQUIRE(graph.Execute(cmd));
    }

    GraphFrameData MakeSkyFrame(const ViewTargets& targets)
    {
        GraphFrameData frame;
        frame.View = HM::Matrix4x4::LookAt(HM::Vector4(5.0f, 1.0f, 2.0f, 1.0f), HM::Vector4(6.0f, 1.0f, 2.0f, 1.0f),
                                           HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
        // 60 degrees, y flipped for Vulkan's clip space as the engine's cameras build it.
        frame.Proj       = HM::Matrix4x4::Perspective(1.0471976f, 1.0f, 0.1f, 100.0f);
        frame.Proj[1][1] = -frame.Proj[1][1];
        frame.Environment.Uniform.Intensity   = 2.0f;
        frame.Environment.Uniform.RotationCos = 0.0f;
        frame.Environment.Uniform.RotationSin = 1.0f;
        frame.Environment.Radiance            = &targets.Cube;
        frame.Environment.ShowSkybox          = true;
        return frame;
    }

    // Where clip-space point (x, y) at the far plane looks, through the pushed inverse.
    HM::Vector3 PushedDirection(const SkyboxPushConstants& constants, float x, float y)
    {
        HM::Matrix4x4 inverse;
        std::memcpy(inverse.GetBuffer(), constants.InverseViewProj, sizeof(constants.InverseViewProj));
        const HM::Vector4 world = inverse * HM::Vector4(x, y, 1.0f, 1.0f);
        const HM::Vector3 d(world.x() / world.w(), world.y() / world.w(), world.z() / world.w());
        const float       length = std::sqrt(d.x() * d.x() + d.y() * d.y() + d.z() * d.z());
        return HM::Vector3(d.x() / length, d.y() / length, d.z() / length);
    }
}

TEST_CASE("Skybox - one fullscreen triangle of the radiance cube over the HDR target, depth read and loaded")
{
    ViewTargets       targets;
    GraphFrameData    frame = MakeSkyFrame(targets);
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    RecordingCommandList cmd;
    RecordSkybox(&context, targets, cmd);

    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "push 80" });
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{ &services.GetPipeline(EnginePipeline::Skybox) });
    CHECK(services.SampledTextures == std::vector<const RHI::IRHITexture*>{ &targets.Cube });
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ &services.GetSampledTextureSet() });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3 });
    CHECK(cmd.EndRenderingCount == 1);

    // Drawn over what Forward rendered, against the prepass depth, both kept.
    REQUIRE(cmd.Renderings.size() == 1);
    const RHI::RenderingInfo& info = cmd.Renderings[0];
    REQUIRE(info.ColorAttachments.size() == 1);
    CHECK(info.ColorAttachments[0].Texture == &targets.Hdr);
    CHECK(info.ColorAttachments[0].LoadOp == RHI::LoadOp::Load);
    REQUIRE(info.DepthAttachment.has_value());
    CHECK(info.DepthAttachment->Texture == &targets.Depth);
    CHECK(info.DepthAttachment->LoadOp == RHI::LoadOp::Load);

    REQUIRE(cmd.PushedData.size() == 1);
    REQUIRE(cmd.PushedData[0].size() == sizeof(SkyboxPushConstants));
    SkyboxPushConstants pushed;
    std::memcpy(&pushed, cmd.PushedData[0].data(), sizeof(pushed));
    CHECK(pushed.Intensity == 2.0f);
    CHECK(pushed.RotationCos == 0.0f);
    CHECK(pushed.RotationSin == 1.0f);
}

TEST_CASE("Skybox - nothing without an environment, with the skybox off, or without a frame context")
{
    ViewTargets targets;
    FakeServices services;

    SUBCASE("No environment")
    {
        GraphFrameData    frame; // the default: no environment, no skybox
        GraphFrameContext context{ &services, &frame };
        RecordingCommandList cmd;
        RecordSkybox(&context, targets, cmd);
        CHECK(cmd.Commands.empty());
        CHECK(cmd.Renderings.empty());
    }
    SUBCASE("ShowSkybox off")
    {
        GraphFrameData frame         = MakeSkyFrame(targets);
        frame.Environment.ShowSkybox = false;
        GraphFrameContext    context{ &services, &frame };
        RecordingCommandList cmd;
        RecordSkybox(&context, targets, cmd);
        CHECK(cmd.Commands.empty());
    }
    SUBCASE("No frame context")
    {
        RecordingCommandList cmd;
        RecordSkybox(nullptr, targets, cmd);
        CHECK(cmd.Commands.empty());
    }
    CHECK(services.SampledTextures.empty());
}

TEST_CASE("Skybox - its slots, and directions that ignore the camera's position")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("Skybox") != nullptr);
    CHECK(registry.Find("Skybox")->Slots == std::vector<std::string>{ "color", "depth" });

    // A camera at (5, 1, 2) looking along +X: the screen's centre looks along +X wherever it stands.
    ViewTargets               targets;
    const GraphFrameData      frame = MakeSkyFrame(targets);
    const SkyboxPushConstants near  = MakeSkyboxPushConstants(frame.View, frame.Proj, frame.Environment.Uniform);
    const HM::Vector3         centre = PushedDirection(near, 0.0f, 0.0f);
    CHECK(centre.x() == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(centre.y() == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(centre.z() == doctest::Approx(0.0f).epsilon(1e-4));

    const HM::Matrix4x4 farView = HM::Matrix4x4::LookAt(HM::Vector4(-300.0f, 40.0f, 9.0f, 1.0f),
                                                        HM::Vector4(-299.0f, 40.0f, 9.0f, 1.0f),
                                                        HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
    const SkyboxPushConstants far = MakeSkyboxPushConstants(farView, frame.Proj, frame.Environment.Uniform);
    for (const float x : { -0.9f, 0.0f, 0.7f })
    {
        for (const float y : { -0.8f, 0.0f, 0.5f })
        {
            const HM::Vector3 a = PushedDirection(near, x, y);
            const HM::Vector3 b = PushedDirection(far, x, y);
            CHECK(a.x() == doctest::Approx(b.x()).epsilon(1e-4));
            CHECK(a.y() == doctest::Approx(b.y()).epsilon(1e-4));
            CHECK(a.z() == doctest::Approx(b.z()).epsilon(1e-4));
        }
    }
    // The top of the screen looks up from the horizon.
    CHECK(PushedDirection(near, 0.0f, -0.9f).z() > 0.1f);
}
