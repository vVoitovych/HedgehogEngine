#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/ParticlesPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t TARGET_SIZE = 64;

    // A view's HDR target and depth, the pool's buffers, and two emitter instances: an alpha one of
    // three particles without a texture in slot row 2 and an additive one with a texture in row 5.
    struct ParticleView
    {
        TestTexture Hdr{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::R16G16B16A16Float,
                                           RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
        TestTexture Depth{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::D32Float, RHI::TextureUsage::DepthStencil } };
        std::array<TestBuffer, PARTICLE_BUFFER_COUNT> Buffers{ TestBuffer(256), TestBuffer(256), TestBuffer(256),
                                                               TestBuffer(256), TestBuffer(1024), TestBuffer(512),
                                                               TestBuffer(1024) };
        FakeDescriptorSet SmokeTexture;
        FakeDescriptorSet White;

        std::vector<ParticleEmitterUniform>        Emitters = std::vector<ParticleEmitterUniform>(2);
        std::vector<uint8_t>                       Reset    = { 0, 0 };
        std::vector<const RHI::IRHIDescriptorSet*> Textures = { nullptr, &SmokeTexture };
        ParticleFrameData                          Particles;
        GraphFrameData                             Frame;

        ParticleView()
        {
            Emitters[0].SlotRow   = 2;
            Emitters[0].SlotCount = 3;
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
        view.Frame.ParticleBuffers.Sort     = graph.ImportBuffer("ParticleSort", 512, false, written);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.State, &view.Buffers[0]);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.AliveOut, &view.Buffers[3]);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.Slots, &view.Buffers[4]);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.Sort, &view.Buffers[5]);
        view.Frame.ParticleBuffers.CollisionDepth = graph.ImportBuffer("ParticleCollisionDepth", 1024, false, written);
        graph.BindImportedBuffer(view.Frame.ParticleBuffers.CollisionDepth, &view.Buffers[6]);

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
        // The alpha emitter's sort, 3 particles in a list of 4: its keys, then the network's 3 steps.
        "pipeline", "set 0", "push 32", "dispatch 1 1 1",
        "pipeline", "set 0", "push 32", "dispatch 1 1 1", "push 32", "dispatch 1 1 1", "push 32", "dispatch 1 1 1",
        "begin", "pipeline", "set 0", "set 1", "push 112", "draw indirect 144 1 16",  // row 2: 2 * 64 + 16
        "pipeline", "set 0", "set 1", "push 112", "draw indirect 336 1 16" });        // row 5
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{
        &services.GetComputePipeline(EngineComputePipeline::ParticleSortKeys),
        &services.GetComputePipeline(EngineComputePipeline::ParticleSortStep),
        &services.GetPipeline(EnginePipeline::ParticlesAlpha), &services.GetPipeline(EnginePipeline::ParticlesAdditive) });
    // The simulation's set for the sort and the draws, the white set for the untextured emitter and
    // its texture for the other.
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{
        services.GetParticleSet(), services.GetParticleSet(), services.GetParticleSet(), &view.White,
        services.GetParticleSet(), &view.SmokeTexture });
    // Its sort pushes the list's length and the steps (k, j) in order.
    const auto sortPush = [&](size_t index)
    {
        ParticleSortPushConstants push;
        REQUIRE(cmd.PushedData.at(index).size() == sizeof(push));
        std::memcpy(&push, cmd.PushedData[index].data(), sizeof(push));
        return push;
    };
    CHECK(sortPush(0).Count == 4);
    CHECK(sortPush(0).EmitterIndex == 0);
    CHECK((sortPush(1).StepK == 2 && sortPush(1).StepJ == 1));
    CHECK((sortPush(2).StepK == 4 && sortPush(2).StepJ == 2));
    CHECK((sortPush(3).StepK == 4 && sortPush(3).StepJ == 1));
    CHECK(sortPush(3).Eye[1] == doctest::Approx(-5.0f));
    CHECK(PushedConstants(cmd, 4).EmitterIndex == 0);
    CHECK(PushedConstants(cmd, 4).HasTexture == 0);
    CHECK(PushedConstants(cmd, 5).EmitterIndex == 1);
    CHECK(PushedConstants(cmd, 5).HasTexture == 1);

    // Over the loaded HDR target against the view's depth, which is read, not written.
    REQUIRE(cmd.Renderings.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Texture == &view.Hdr);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Load);
    REQUIRE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.Renderings[0].DepthAttachment->Texture == &view.Depth);

    // The buffers the simulation wrote are read: the states and alive list by the sort and the vertex
    // shader, the slot rows by the sort and then as draw arguments; the sort list is written by the
    // sort, a barrier after each of its dispatches, and read by the draw.
    bool sawArgs = false, sawState = false, sawSorted = false;
    int  sortSteps = 0;
    for (const RecordingCommandList::BarrierCall& call : cmd.Calls)
        for (const RHI::BufferBarrier& barrier : call.BufferBarriers)
        {
            if (barrier.Buffer == &view.Buffers[4] && barrier.After == RHI::ResourceState::IndirectArgument)
                sawArgs = barrier.Before == RHI::ResourceState::ShaderResource;
            if (barrier.Buffer == &view.Buffers[0])
                sawState = sawState || barrier.After == RHI::ResourceState::ShaderResource;
            if (barrier.Buffer == &view.Buffers[5] && barrier.Before == RHI::ResourceState::UnorderedAccess)
            {
                sortSteps += barrier.After == RHI::ResourceState::UnorderedAccess ? 1 : 0;
                sawSorted = sawSorted || barrier.After == RHI::ResourceState::ShaderResource;
            }
        }
    CHECK(sawArgs);
    CHECK(sawState);
    CHECK(sawSorted);
    CHECK(sortSteps >= 4); // after the keys and each of the 3 steps
}

TEST_CASE("Particles - the shadow view copies its depth when an emitter collides with it")
{
    ParticleView view;
    view.Emitters[1].Collision = PARTICLE_COLLISION_DEPTH_BUFFER;
    view.Frame.Particles       = &view.Particles;
    ParticleCollisionDepth collision;
    view.Frame.ParticleCollision = &collision;
    FakeServices services;
    services.MakeParticleSet();
    GraphFrameContext context{ &services, &view.Frame };

    RecordingCommandList cmd;
    RecordParticles(&context, view, cmd);

    // First the copy: its pipeline, the simulation's set and the depth's, one thread per texel.
    REQUIRE(cmd.Commands.size() > 4);
    CHECK(std::vector<std::string>(cmd.Commands.begin(), cmd.Commands.begin() + 4)
          == std::vector<std::string>{ "pipeline", "set 0", "set 1", "dispatch 64 64 1" });
    CHECK(cmd.BoundPipelines.front() == &services.GetComputePipeline(EngineComputePipeline::ParticleCopyDepth));
    CHECK(cmd.BoundSets[0] == services.GetParticleSet());
    CHECK(cmd.BoundSets[1] == &services.GetSampledTextureSet());
    REQUIRE(services.SampledTextures.size() == 1);
    CHECK(services.SampledTextures[0] == &view.Depth);
    // The depth is sampled, then the collision depth written, before the draw reads the depth.
    bool sampled = false, written = false;
    for (const RecordingCommandList::BarrierCall& call : cmd.Calls)
    {
        for (const RHI::TextureBarrier& barrier : call.TextureBarriers)
            sampled = sampled || (barrier.Texture == &view.Depth && barrier.After == RHI::ResourceState::ShaderResource);
        for (const RHI::BufferBarrier& barrier : call.BufferBarriers)
            written = written || barrier.Buffer == &view.Buffers[6];
    }
    CHECK(sampled);
    CHECK(written);
    // And the camera it saw the depth with is kept for the next frame.
    CHECK(collision.Written);
    const HM::Matrix4x4 viewProj = view.Frame.Proj * view.Frame.View;
    for (int i = 0; i < 16; ++i)
        CHECK(collision.ViewProj.GetBuffer()[i] == doctest::Approx(viewProj.GetBuffer()[i]));
}

TEST_CASE("Particles - no depth copy on another view or without a depth-colliding emitter")
{
    for (const bool shadowView : { false, true })
    {
        ParticleView view;
        view.Emitters[1].Collision = shadowView ? PARTICLE_COLLISION_HEIGHT_PLANE : PARTICLE_COLLISION_DEPTH_BUFFER;
        view.Frame.Particles       = &view.Particles;
        ParticleCollisionDepth collision;
        view.Frame.ParticleCollision = shadowView ? &collision : nullptr;
        FakeServices services;
        services.MakeParticleSet();
        GraphFrameContext context{ &services, &view.Frame };

        RecordingCommandList cmd;
        RecordParticles(&context, view, cmd);
        CHECK(std::count(cmd.Commands.begin(), cmd.Commands.end(), std::string("dispatch 64 64 1")) == 0);
        CHECK(services.SampledTextures.empty());
        CHECK_FALSE(collision.Written);
    }
}

TEST_CASE("Particles - additive emitters alone draw unsorted")
{
    ParticleView view;
    view.Emitters[0].Blend = 1;
    view.Frame.Particles   = &view.Particles;
    FakeServices services;
    services.MakeParticleSet();
    GraphFrameContext context{ &services, &view.Frame };

    RecordingCommandList cmd;
    RecordParticles(&context, view, cmd);
    CHECK(cmd.Commands.front() == "begin");
    CHECK(std::count(cmd.Commands.begin(), cmd.Commands.end(), std::string("dispatch 1 1 1")) == 0);
}

TEST_CASE("Particles - the bitonic steps sort any list of a power-of-two length, and padded lists too")
{
    const auto sortOnCpu = [](std::vector<uint32_t>& keys)
    {
        const uint32_t length = static_cast<uint32_t>(keys.size());
        ForEachBitonicStep(length, [&](uint32_t k, uint32_t j)
        {
            // What SortStep.comp does, every thread of the step at once.
            for (uint32_t i = 0; i < length; ++i)
            {
                const uint32_t other = i ^ j;
                if (other <= i)
                    continue;
                const bool ascending = (i & k) == 0;
                if ((keys[i] > keys[other]) == ascending)
                    std::swap(keys[i], keys[other]);
            }
        });
    };

    std::vector<uint32_t> permutation = { 0, 1, 2, 3, 4, 5, 6, 7 };
    do
    {
        std::vector<uint32_t> keys = permutation;
        sortOnCpu(keys);
        CHECK(std::is_sorted(keys.begin(), keys.end()));
    } while (std::next_permutation(permutation.begin(), permutation.end()));

    // 1000 keys in a list of 1024, padded as SortKeys.comp pads it, so the padding sorts last.
    std::mt19937          random(7);
    std::vector<uint32_t> keys(ParticleSortLength(1000), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < 1000; ++i)
        keys[i] = random() % 0xFFFFFFFEu;
    sortOnCpu(keys);
    CHECK(std::is_sorted(keys.begin(), keys.end()));
    CHECK(keys[999] != 0xFFFFFFFFu);
    CHECK(keys[1000] == 0xFFFFFFFFu);

    CHECK(ParticleSortLength(0) == 1);
    CHECK(ParticleSortLength(1) == 1);
    CHECK(ParticleSortLength(3) == 4);
    CHECK(ParticleSortLength(1024) == 1024);
    int steps = 0;
    ForEachBitonicStep(1024, [&](uint32_t, uint32_t) { ++steps; });
    CHECK(steps == 10 * 11 / 2);
}

TEST_CASE("Particles - the sort's eye is the view's position in the world")
{
    const HM::Matrix4x4 view = HM::Matrix4x4::LookAt(HM::Vector4(3.0f, -5.0f, 2.0f, 1.0f), HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f),
                                                     HM::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
    const ParticleSortPushConstants push = MakeParticleSortPushConstants(view, 2, 64);
    CHECK(push.Eye[0] == doctest::Approx(3.0f));
    CHECK(push.Eye[1] == doctest::Approx(-5.0f));
    CHECK(push.Eye[2] == doctest::Approx(2.0f));
    CHECK(push.EmitterIndex == 2);
    CHECK(push.Count == 64);
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
