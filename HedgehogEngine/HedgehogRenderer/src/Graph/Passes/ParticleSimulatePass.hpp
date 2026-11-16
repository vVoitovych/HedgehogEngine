#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

namespace Renderer
{
    class RenderGraphRuntime;

    // The shared phase's particle simulation, once per frame before any view: for every emitter
    // instance of particles, its slot started afresh when it asks (Reset), its newborns spawned (Emit),
    // its alive particles moved on (Simulate), then every emitter's counters and draw arguments made
    // ready for the next frame (Finish), with a buffer barrier between the stages. It imports the
    // pool's buffers from the states the frames before left them in and writes them all, and it is
    // never culled: particles age whether or not a view draws them. Returns the buffers as it left
    // them, for the views to draw from.
    ParticleGraphBuffers DeclareParticleSimulation(RenderGraphRuntime& graph, IGraphPassServices& services,
                                                   const ParticleFrameData& particles);

    // The names the pool's buffers are imported under, in ParticleBuffer order.
    inline constexpr std::array<const char*, PARTICLE_BUFFER_COUNT> PARTICLE_BUFFER_NAMES = {
        "ParticleState", "ParticleDeadList", "ParticleAliveIn", "ParticleAliveOut", "ParticleSlots", "ParticleSort",
        "ParticleCollisionDepth"
    };

    // Push constants of every particle compute shader (Particles/Common.glsl's Push).
    struct ParticlePushConstants
    {
        uint32_t EmitterIndex = 0;
        uint32_t EmitterCount = 0;
        uint32_t Padding[2]   = {};
    };
    static_assert(sizeof(ParticlePushConstants) <= 32, "ParticleCompute.pl's push constant range is 32 bytes.");

    // Threads per work group of the particle compute shaders, and the groups count threads need.
    inline constexpr uint32_t PARTICLE_GROUP_SIZE = 64;
    [[nodiscard]] constexpr uint32_t ParticleGroups(uint32_t threads)
    {
        return (threads + PARTICLE_GROUP_SIZE - 1) / PARTICLE_GROUP_SIZE;
    }
}
