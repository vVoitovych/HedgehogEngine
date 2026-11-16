#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <bit>
#include <cstdint>

namespace Renderer
{
    // The Particles pipelines' push constants (Particles/Billboard.glsl's DrawPush).
    struct ParticlesPushConstants
    {
        float    ViewProj[16] = {};
        float    CameraRight[4] = {}; // xyz: the view's right in the world
        float    CameraUp[4]    = {}; // xyz: the view's up in the world
        uint32_t EmitterIndex   = 0;
        uint32_t HasTexture     = 0;
        uint32_t Padding[2]     = {};
    };
    static_assert(sizeof(ParticlesPushConstants) == 112, "Particles.pl's push constant range is 112 bytes.");

    // The push constants for one emitter of a view: its viewProj and the camera's right and up in the
    // world, read from the view matrix's rows.
    [[nodiscard]] ParticlesPushConstants MakeParticlesPushConstants(const HM::Matrix4x4& view, const HM::Matrix4x4& proj,
                                                                    uint32_t emitterIndex, bool hasTexture);

    // The sort's push constants (Particles/Sort.glsl's SortPush).
    struct ParticleSortPushConstants
    {
        uint32_t EmitterIndex = 0;
        uint32_t Count        = 0; // the sort list's length, a power of two
        uint32_t StepJ        = 0;
        uint32_t StepK        = 0;
        float    Eye[4]       = {}; // xyz: the view's position in the world
    };
    static_assert(sizeof(ParticleSortPushConstants) == 32, "ParticleCompute.pl's push constant range is 32 bytes.");

    // The sort's push constants for one emitter of a view, its eye taken from the view matrix.
    [[nodiscard]] ParticleSortPushConstants MakeParticleSortPushConstants(const HM::Matrix4x4& view, uint32_t emitterIndex,
                                                                          uint32_t count);

    // How long an emitter's sort list is: its slot's particles rounded up to a power of two.
    [[nodiscard]] constexpr uint32_t ParticleSortLength(uint32_t slotCount)
    {
        return std::bit_ceil(slotCount == 0 ? 1u : slotCount);
    }

    // Calls step(k, j) for every step of a bitonic sort of length (a power of two) elements, in
    // order: k the length of the sequences being merged, j the distance compared across.
    template <typename Step>
    constexpr void ForEachBitonicStep(uint32_t length, Step&& step)
    {
        for (uint32_t k = 2; k <= length; k <<= 1)
            for (uint32_t j = k >> 1; j > 0; j >>= 1)
                step(k, j);
    }

    // Where an emitter's DrawIndirect arguments sit in the slot buffer (Particles/Common.glsl's SlotRow).
    [[nodiscard]] constexpr size_t ParticleDrawArgsOffset(uint32_t slotRow)
    {
        return size_t{ slotRow } * 64 + 16;
    }

    // Particles: slots color and depth (read-only). Draws the frame's particles (GraphFrameData's
    // Particles, as the shared phase's simulation left them) over color as camera-facing billboards,
    // each emitter instance one DrawIndirect from its slot row: alpha-blended or additive by its blend,
    // its texture or a soft round dot, depth tested against the view's depth and not written. When
    // an emitter is alpha-blended it first adds a compute pass, "<name> sort", sorting each such
    // emitter's alive particles back to front from the view's eye into the sort list (keys, then the
    // bitonic network's steps, a barrier after each), which the draw reads; additive emitters draw
    // unsorted. On the shadow view's frame (GraphFrameData::ParticleCollision), when an emitter
    // collides with the depth buffer, it first adds "<name> collision depth", copying the view's depth
    // into the collision depth for the next frame's simulation and recording the view-projection it
    // saw it with. It declares the reads of the particle buffers, so it runs after the simulation;
    // without particles it records nothing.
    [[nodiscard]] PassTypeInfo GetParticlesPassType();
}
