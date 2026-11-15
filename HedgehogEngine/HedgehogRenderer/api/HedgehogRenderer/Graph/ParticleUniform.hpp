#pragma once

#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>

namespace HX
{
    struct ParticleEmitterInstance;
}

namespace Renderer
{
    // A range of the shared particle buffers one emitter instance owns: particles First to
    // First + Count - 1.
    struct ParticleSlot
    {
        uint32_t First = 0;
        uint32_t Count = 0;

        bool operator==(const ParticleSlot&) const = default;
    };

    // The baked rows, four samples to a vec4 (std140 gives a float array a 16-byte stride).
    inline constexpr uint32_t PARTICLE_CURVE_VEC4S = HedgehogEngine::PARTICLE_CURVE_SAMPLES / 4;

    // Everything the particle compute and draw shaders read of one emitter instance for one frame,
    // laid out for std140 (Particles/Common.glsl's EmitterData).
    struct ParticleEmitterUniform
    {
        // Newborn particles are placed by EmitMatrix into the simulation space (the entity's world
        // matrix for World space, identity for Local); DrawMatrix takes the simulation space to the
        // world (identity for World, the entity's matrix for Local), so a Local effect moves with it.
        HM::Matrix4x4 EmitMatrix;                  // offset 0
        HM::Matrix4x4 DrawMatrix;                  // 64
        float StartColor[4]  = {};                 // 128, linear RGBA
        float Gravity[3]     = {};                 // 144
        float Drag           = 0.0f;
        float BoxSize[3]     = {};                 // 160
        float Radius         = 0.0f;
        float LifetimeMin    = 0.0f;               // 176
        float LifetimeMax    = 0.0f;
        float SpeedMin       = 0.0f;
        float SpeedMax       = 0.0f;
        float SizeMin        = 0.0f;               // 192
        float SizeMax        = 0.0f;
        float RotationMin    = 0.0f;               // radians
        float RotationMax    = 0.0f;
        float ConeAngle      = 0.0f;               // 208, radians from +Z
        float DeltaTime      = 0.0f;
        float SoftDistance   = 0.0f;
        float SheetFps       = 0.0f;
        float PlaneHeight    = 0.0f;               // 224
        float Bounce         = 0.0f;
        float LifetimeLoss   = 0.0f;
        float Padding0       = 0.0f;
        uint32_t SlotFirst   = 0;                  // 240
        uint32_t SlotCount   = 0;
        uint32_t SpawnCount  = 0;
        uint32_t Seed        = 0;
        uint32_t Shape       = 0;                  // 256, HedgehogEngine's ParticleShape
        uint32_t Blend       = 0;                  // ParticleBlend
        uint32_t Space       = 0;                  // ParticleSpace
        uint32_t Collision   = 0;                  // ParticleCollisionMode
        uint32_t SheetTilesX = 1;                  // 272
        uint32_t SheetTilesY = 1;
        uint32_t EmitFromSurface = 0;
        uint32_t Padding1    = 0;
        float SizeOverLife[PARTICLE_CURVE_VEC4S][4]  = {};                              // 288
        float SpeedOverLife[PARTICLE_CURVE_VEC4S][4] = {};                              // 416
        float ColorOverLife[HedgehogEngine::PARTICLE_CURVE_SAMPLES][4] = {};             // 544
    };
    static_assert(sizeof(ParticleEmitterUniform) == 1056, "ParticleEmitterUniform must match Common.glsl's std140 block.");

    // The uniform for one emitter of an effect as one instance plays it this frame in slot: the
    // emitter's settings (its degrees made radians), the instance's matrix, spawn count, seed and
    // frame time. A non-finite number becomes 0.
    [[nodiscard]] ParticleEmitterUniform MakeParticleEmitterUniform(const HedgehogEngine::ParticleEmitterView& emitter,
                                                                    const HX::ParticleEmitterInstance&       instance,
                                                                    ParticleSlot                              slot);
}
