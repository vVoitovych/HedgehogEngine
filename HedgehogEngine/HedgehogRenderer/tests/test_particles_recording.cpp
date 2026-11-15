#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/ParticlesPass.hpp"

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
    constexpr uint32_t TARGET_SIZE = 64;

    // A view's HDR target and depth, the pool's buffers, and two emitter instances: an alpha one
    // without a texture in slot row 2 and an additive one with a texture in row 5.
    struct ParticleView
    {
        TestTexture Hdr{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::R16G16B16A16Float,
                                           RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
        TestTexture Depth{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::D32Float, RHI::TextureUsage::DepthStencil } };
        std::array<TestBuffer, PARTICLE_BUFFER_COUNT> Buffers{ TestBuffer(256), TestBuffer(256), TestBuffer(256),
                                                               TestBuffer(256), TestBuffer(1024) };
        FakeDescriptorSet SmokeTexture;
        FakeDescriptorSet White;

        std::vector<ParticleEmitterUniform>        Emitters = std::vector<ParticleEmitterUniform>(2);
        std::vector<uint8_t>                       Reset    = { 0, 0 };
        std::vector<const RHI::IRHIDescriptorSet*> Textures = { nullptr, &SmokeTexture };
        ParticleFrameData                          Particles;
        GraphFrameData                             Frame;

        ParticleView()
        {
            Emitters[0].SlotRow = 2;
            Emitters[0].Blend   = 0; // Alpha
            Emitters[1].SlotRow = 5;
            Emitters[1].Blend   = 1; // Additive
            for (size_t i = 0; i < PARTICLE_BUFFER_COUNT; ++i)
                Particles.Buffers[i] = &Buffers[i];
            Particles.Emitters = Emitters;
            Particles.Reset    = Reset;
            Particles.Textures = Textures;

            Frame.View = HM::Matrix4x4::LookAt(HM::Vector4(0.0f, -5.0f, 1.0f, 1.0f), HM::Vector4(0.0f, 0.0f, 1.0f, 1.0f),
                                               HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
            Frame.Proj           = HM::Matrix4x4::Perspective(1.0471976f, 1.0f, 0.1f, 100.0f);
            Frame.UiSolidTexture = &White;
        }
    };

    // A lone Particles pass over the view's targets, the pool's buffers imported as the simulation
    // left them (written), run with context into cmd.
    void RecordParticles(const GraphFrameContext* context, ParticleView& view, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(context);

        const RGTexture hdr   = graph.ImportTexture("hdr", RHI::Format::R16G16B16A16Float, false);
        const RGTexture depth = graph.ImportTexture("depth", RHI::Format::D32Float, true);
        graph.BindImportedTexture(hdr, &view.Hdr);
        graph.BindImportedTexture(depth, &view.Depth);
        const RHI::ResourceState written = RHI::ResourceState::UnorderedAccess;
        view.Frame.ParticleBuffers.State    = graph.ImportBuffer("ParticleState", 256, false, written);
        view.Frame.ParticleBuffers.AliveOut = graph.ImportBuffer("ParticleAliveOut", 256, false, written);
        view.Frame.ParticleBuffers.Slots    = graph.ImportBuffer("ParticleSlots", 1024, false, written);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.State, &view.Buffers[0]);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.AliveOut, &view.Buffers[3]);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.Slots, &view.Buffers[4]);

        PassInvocation particles("Particles");
        particles.SetSlot("color", hdr);
        particles.SetSlot("depth", depth);
        registry.Find("Particles")->Build(graph, particles);
        graph.BindOutput(graph.AddOutputSlot("hdr", RHI::Format::R16G16B16A16Float,
                                             RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE)),
                         particles.GetSlot("color"));
        REQUIRE(graph.Execute(cmd));
    }

    ParticlesPushConstants PushedConstants(const RecordingCommandList& cmd, size_t index)
    {
        ParticlesPushConstants constants;
        REQUIRE(cmd.PushedData.at(index).size() == sizeof(constants));
        std::memcpy(&constants, cmd.PushedData[index].data(), sizeof(constants));
        return constants;
    }
}

TEST_CASE("Particles - each emitter one indirect draw from its slot row, with its blend's pipeline and its texture")
{
    ParticleView view;
    view.Frame.Particles = &view.Particles;
    FakeServices services;
    services.MakeParticleSet();
    GraphFrameContext context{ &services, &view.Frame };

    RecordingCommandList cmd;
    RecordParticles(&context, view, cmd);

    CHECK(cmd.Commands == std::vector<std::string>{
        "begin", "pipeline", "set 0", "set 1", "push 112", "draw indirect 144 1 16",  // row 2: 2 * 64 + 16
        "pipeline", "set 0", "set 1", "push 112", "draw indirect 336 1 16" });        // row 5
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{
        &services.GetPipeline(EnginePipeline::ParticlesAlpha), &services.GetPipeline(EnginePipeline::ParticlesAdditive) });
    // The simulation's set, then the white set for the untextured emitter and its texture for the other.
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ services.GetParticleSet(), &view.White,
                                                                        services.GetParticleSet(), &view.SmokeTexture });
    CHECK(PushedConstants(cmd, 0).EmitterIndex == 0);
    CHECK(PushedConstants(cmd, 0).HasTexture == 0);
    CHECK(PushedConstants(cmd, 1).EmitterIndex == 1);
    CHECK(PushedConstants(cmd, 1).HasTexture == 1);

    // Over the loaded HDR target against the view's depth, which is read, not written.
    REQUIRE(cmd.Renderings.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Texture == &view.Hdr);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Load);
    REQUIRE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.Renderings[0].DepthAttachment->Texture == &view.Depth);

    // The buffers the simulation wrote are read: the states and alive list by the vertex shader,
    // the slot rows as draw arguments.
    REQUIRE_FALSE(cmd.Calls.empty());
    bool sawArgs = false, sawState = false;
    for (const RHI::BufferBarrier& barrier : cmd.Calls[0].BufferBarriers)
    {
        if (barrier.Buffer == &view.Buffers[4])
            sawArgs = barrier.Before == RHI::ResourceState::UnorderedAccess && barrier.After == RHI::ResourceState::IndirectArgument;
        if (barrier.Buffer == &view.Buffers[0])
            sawState = barrier.After == RHI::ResourceState::ShaderResource;
    }
    CHECK(sawArgs);
    CHECK(sawState);
}

TEST_CASE("Particles - nothing without particles, a frame context or the simulation's set")
{
    ParticleView view;
    FakeServices services;
    services.MakeParticleSet();
    GraphFrameContext context{ &services, &view.Frame };

    RecordingCommandList noParticles;
    RecordParticles(&context, view, noParticles);
    CHECK(noParticles.Commands.empty());

    view.Frame.Particles = &view.Particles;
    RecordingCommandList noContext;
    RecordParticles(nullptr, view, noContext);
    CHECK(noContext.Commands.empty());

    FakeServices      unsimulated;
    GraphFrameContext unsimulatedContext{ &unsimulated, &view.Frame };
    RecordingCommandList noSet;
    RecordParticles(&unsimulatedContext, view, noSet);
    CHECK(noSet.Commands.empty());
}

TEST_CASE("Particles - the push constants carry the view's camera axes and its viewProj")
{
    const HM::Matrix4x4 view = HM::Matrix4x4::LookAt(HM::Vector4(0.0f, -5.0f, 0.0f, 1.0f), HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f),
                                                     HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
    const HM::Matrix4x4 proj = HM::Matrix4x4::Perspective(1.0f, 1.0f, 0.1f, 10.0f);
    const ParticlesPushConstants constants = MakeParticlesPushConstants(view, proj, 3, true);
    // Looking along +Y with +Z up: right is +X and up is +Z (up to the view's handedness, unit length).
    CHECK(std::abs(constants.CameraRight[0]) == doctest::Approx(1.0f));
    CHECK(constants.CameraRight[2] == doctest::Approx(0.0f));
    CHECK(constants.CameraUp[2] == doctest::Approx(1.0f));
    CHECK(constants.CameraUp[0] == doctest::Approx(0.0f));
    const HM::Matrix4x4 viewProj = proj * view;
    CHECK(std::memcmp(constants.ViewProj, viewProj.GetBuffer(), sizeof(constants.ViewProj)) == 0);
    CHECK(constants.EmitterIndex == 3);
    CHECK(constants.HasTexture == 1);
    CHECK(ParticleDrawArgsOffset(0) == 16);
    CHECK(ParticleDrawArgsOffset(3) == 3 * 64 + 16);
}
