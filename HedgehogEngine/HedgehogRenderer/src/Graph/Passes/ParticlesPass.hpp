#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include "HedgehogMath/api/Matrix.hpp"

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

    // Where an emitter's DrawIndirect arguments sit in the slot buffer (Particles/Common.glsl's SlotRow).
    [[nodiscard]] constexpr size_t ParticleDrawArgsOffset(uint32_t slotRow)
    {
        return size_t{ slotRow } * 64 + 16;
    }

    // Particles: slots color and depth (read-only). Draws the frame's particles (GraphFrameData's
    // Particles, as the shared phase's simulation left them) over color as camera-facing billboards,
    // each emitter instance one DrawIndirect from its slot row: alpha-blended or additive by its blend,
    // its texture or a soft round dot, depth tested against the view's depth and not written. It
    // declares the reads of the particle buffers, so it runs after the simulation; without particles
    // it records nothing.
    [[nodiscard]] PassTypeInfo GetParticlesPassType();
}
