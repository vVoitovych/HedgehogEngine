#include "../src/Graph/Passes/ParticleSimulatePass.hpp"
#include "../src/Particles/ParticleResources.hpp"

#include "HedgehogRenderer/Frame/SharedPhase.hpp"
#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cstring>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    // The pool's buffers and two emitters: the first starts afresh and spawns nothing, the second
    // spawns 70 of its 200 particles.
    struct ParticleFrameFixture
    {
        TestBuffer State{ 1024 };
        TestBuffer DeadList{ 1024 };
        TestBuffer AliveIn{ 1024 };
        TestBuffer AliveOut{ 1024 };
        TestBuffer Slots{ 1024 };
        TestBuffer Sort{ 1024 };

        std::vector<ParticleEmitterUniform> Emitters;
        std::vector<uint8_t>                Reset;
        ParticleFrameData                   Frame;

        ParticleFrameFixture(RHI::ResourceState initialState = RHI::ResourceState::Undefined)
        {
            Emitters.resize(2);
            Emitters[0].SlotCount  = 100;
            Emitters[1].SlotCount  = 200;
            Emitters[1].SpawnCount = 70;
            Reset                  = { 1, 0 };
            Frame.Buffers          = { &State, &DeadList, &AliveIn, &AliveOut, &Slots, &Sort };
            Frame.States.fill(initialState);
            Frame.Emitters         = Emitters;
            Frame.Reset            = Reset;
        }
    };

    // The push constants' emitter indices, in order.
    std::vector<uint32_t> PushedEmitters(const RecordingCommandList& cmd)
    {
        std::vector<uint32_t> emitters;
        for (const std::vector<unsigned char>& push : cmd.PushedData)
        {
            ParticlePushConstants constants;
            REQUIRE(push.size() == sizeof(constants));
            std::memcpy(&constants, push.data(), sizeof(constants));
            emitters.push_back(constants.EmitterIndex);
        }
        return emitters;
    }
}

TEST_CASE("Particle simulation - reset, emit, simulate and finish, a barrier between the stages")
{
    TestDevice           device;
    RenderGraphRuntime   graph(device, 64 * 1024);
    FakeServices         services;
    ParticleFrameFixture particles;

    DeclareParticleSimulation(graph, services, particles.Frame);
    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    REQUIRE(graph.GetLastExecutedPassCount() == 1); // a side effect: kept though nothing reads it

    CHECK(cmd.Commands == std::vector<std::string>{
        "pipeline", "set 0", "push 16", "dispatch 2 1 1",                                   // reset emitter 0: 100 particles
        "pipeline", "set 0", "push 16", "dispatch 2 1 1",                                   // emit 70 of emitter 1
        "pipeline", "set 0", "push 16", "dispatch 2 1 1", "push 16", "dispatch 4 1 1",      // simulate both slots
        "pipeline", "set 0", "push 16", "dispatch 1 1 1" });                                // finish both
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{
        &services.GetComputePipeline(EngineComputePipeline::ParticleReset),
        &services.GetComputePipeline(EngineComputePipeline::ParticleEmit),
        &services.GetComputePipeline(EngineComputePipeline::ParticleSimulate),
        &services.GetComputePipeline(EngineComputePipeline::ParticleFinish) });
    CHECK(PushedEmitters(cmd) == std::vector<uint32_t>{ 0, 1, 0, 1, 0 });
    for (const RHI::IRHIDescriptorSet* set : cmd.BoundSets)
        CHECK(set == services.GetParticleSet());
    CHECK(services.ParticleSetUploads == std::vector<size_t>{ 2 });
    CHECK(cmd.Scopes == std::vector<std::string>{ "begin Particles", "end" });

    // The graph's barrier into UnorderedAccess, then one between each stage, on all six buffers.
    REQUIRE(cmd.Calls.size() == 4);
    REQUIRE(cmd.Calls[0].BufferBarriers.size() == PARTICLE_BUFFER_COUNT);
    CHECK(cmd.Calls[0].BufferBarriers[0].Buffer == &particles.State);
    CHECK(cmd.Calls[0].BufferBarriers[0].Before == RHI::ResourceState::Undefined);
    CHECK(cmd.Calls[0].BufferBarriers[0].After == RHI::ResourceState::UnorderedAccess);
    for (size_t call = 1; call < cmd.Calls.size(); ++call)
    {
        REQUIRE(cmd.Calls[call].BufferBarriers.size() == PARTICLE_BUFFER_COUNT);
        for (const RHI::BufferBarrier& barrier : cmd.Calls[call].BufferBarriers)
        {
            CHECK(barrier.Before == RHI::ResourceState::UnorderedAccess);
            CHECK(barrier.After == RHI::ResourceState::UnorderedAccess);
        }
    }
}

TEST_CASE("Particle simulation - without resets or spawns only simulate and finish run, after the last frame's work")
{
    TestDevice           device;
    RenderGraphRuntime   graph(device, 64 * 1024);
    FakeServices         services;
    ParticleFrameFixture particles(RHI::ResourceState::UnorderedAccess);
    particles.Reset                  = { 0, 0 };
    particles.Emitters[1].SpawnCount = 0;
    particles.Frame.Reset            = particles.Reset;

    DeclareParticleSimulation(graph, services, particles.Frame);
    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    CHECK(cmd.Commands == std::vector<std::string>{ "pipeline", "set 0", "push 16", "dispatch 2 1 1", "push 16",
                                                    "dispatch 4 1 1", "pipeline", "set 0", "push 16", "dispatch 1 1 1" });

    // The buffers come from the frame before in UnorderedAccess: still a barrier, so this frame's
    // writes wait for that frame's.
    REQUIRE(cmd.Calls.size() == 2);
    REQUIRE(cmd.Calls[0].BufferBarriers.size() == PARTICLE_BUFFER_COUNT);
    CHECK(cmd.Calls[0].BufferBarriers[0].Before == RHI::ResourceState::UnorderedAccess);
    CHECK(cmd.Calls[0].BufferBarriers[0].After == RHI::ResourceState::UnorderedAccess);
}

TEST_CASE("Particle simulation - the shared phase declares it only with particles")
{
    TestDevice          device;
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    SharedPhase  shared(registry);
    FakeServices services;

    RenderGraphRuntime without(device, 64 * 1024);
    (void)shared.Declare(without, services, nullptr, {}, {});
    RecordingCommandList none;
    REQUIRE(without.Execute(none));
    CHECK(without.GetLastExecutedPassCount() == 0);

    ParticleFrameFixture particles;
    RenderGraphRuntime   with(device, 64 * 1024);
    (void)shared.Declare(with, services, nullptr, {}, {}, &particles.Frame);
    RecordingCommandList cmd;
    REQUIRE(with.Execute(cmd));
    CHECK(with.GetLastExecutedPassCount() == 1);
    CHECK(services.ParticleSetUploads == std::vector<size_t>{ 2 });
}

TEST_CASE("Particle resources - a frame's uniforms, resets and alive lists, the lists swapping every frame")
{
    TestDevice                                       device;
    ParticleResources                                particles;
    std::vector<HedgehogEngine::ParticleEmitterView> storage(1);
    storage[0].maxParticles = 100;
    particles.SetEffect(0, 1, { storage });

    HX::RenderScene scene;
    CHECK(particles.BuildFrame(scene, {}) == nullptr); // nothing extracted

    HX::ParticleEmitterInstance instance;
    instance.InstanceId    = 3;
    instance.SpawnCount    = 12;
    scene.ParticleEmitters = { instance };
    particles.Sync(scene, device);

    const ParticleFrameData* first = particles.BuildFrame(scene, {});
    REQUIRE(first != nullptr);
    REQUIRE(first->Emitters.size() == 1);
    CHECK(first->Emitters[0].SpawnCount == 12);
    CHECK(first->Emitters[0].SlotCount == 100);
    CHECK(first->Reset[0] == 1);
    CHECK(first->States[0] == RHI::ResourceState::Undefined);
    CHECK(first->Get(ParticleBuffer::State) == particles.GetStateBuffer());
    CHECK(first->Textures[0] == nullptr);
    RHI::IRHIBuffer* const firstIn  = first->Get(ParticleBuffer::AliveIn);
    RHI::IRHIBuffer* const firstOut = first->Get(ParticleBuffer::AliveOut);
    CHECK(firstIn != firstOut);

    // The frame left the state buffer read by the draws, its alive lists as the simulation wrote them.
    std::array<std::optional<RHI::ResourceState>, PARTICLE_BUFFER_COUNT> finalStates{};
    finalStates[static_cast<size_t>(ParticleBuffer::State)]    = RHI::ResourceState::ShaderResource;
    finalStates[static_cast<size_t>(ParticleBuffer::AliveIn)]  = RHI::ResourceState::UnorderedAccess;
    finalStates[static_cast<size_t>(ParticleBuffer::AliveOut)] = RHI::ResourceState::ShaderResource;
    finalStates[static_cast<size_t>(ParticleBuffer::Slots)]    = RHI::ResourceState::IndirectArgument;
    particles.SetFinalStates(finalStates);

    particles.Sync(scene, device);
    const ParticleFrameData* second = particles.BuildFrame(scene, {});
    REQUIRE(second != nullptr);
    CHECK(second->Reset[0] == 0);
    CHECK(second->Get(ParticleBuffer::AliveIn) == firstOut); // last frame's survivors
    CHECK(second->Get(ParticleBuffer::AliveOut) == firstIn);
    // Each buffer from its own state: the lists follow their buffers as they swap.
    CHECK(second->States[static_cast<size_t>(ParticleBuffer::State)] == RHI::ResourceState::ShaderResource);
    CHECK(second->States[static_cast<size_t>(ParticleBuffer::DeadList)] == RHI::ResourceState::Undefined);
    CHECK(second->States[static_cast<size_t>(ParticleBuffer::AliveIn)] == RHI::ResourceState::ShaderResource);
    CHECK(second->States[static_cast<size_t>(ParticleBuffer::AliveOut)] == RHI::ResourceState::UnorderedAccess);
    CHECK(second->States[static_cast<size_t>(ParticleBuffer::Slots)] == RHI::ResourceState::IndirectArgument);
}
