#include "HedgehogRenderer/Graph/ParticleUniform.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <numbers>

using namespace Renderer;

namespace
{
    HedgehogEngine::ParticleEmitterView MakeEmitter()
    {
        HedgehogEngine::ParticleEmitterView emitter;
        emitter.maxParticles    = 500;
        emitter.shape           = 2; // Cone
        emitter.blend           = 1; // Additive
        emitter.space           = 1; // World
        emitter.collisionMode   = 1; // HeightPlane
        emitter.emitFromSurface = true;
        emitter.radius          = 0.5f;
        emitter.angle           = 90.0f;
        emitter.boxSize         = { 1.0f, 2.0f, 3.0f };
        emitter.lifetimeMin     = 1.0f;
        emitter.lifetimeMax     = 2.0f;
        emitter.speedMin        = 3.0f;
        emitter.speedMax        = 4.0f;
        emitter.sizeMin         = 0.25f;
        emitter.sizeMax         = 0.5f;
        emitter.rotationMin     = -180.0f;
        emitter.rotationMax     = 180.0f;
        emitter.startColor      = { 1.0f, 0.5f, 0.25f, 1.0f };
        emitter.gravity         = { 0.0f, 0.0f, -9.81f };
        emitter.drag            = 0.1f;
        emitter.sheetTilesX     = 4;
        emitter.sheetTilesY     = 2;
        emitter.sheetFps        = 12.0f;
        emitter.softDistance    = 0.3f;
        emitter.planeHeight     = -1.0f;
        emitter.bounce          = 0.5f;
        emitter.lifetimeLoss    = 0.25f;
        for (uint32_t i = 0; i < HedgehogEngine::PARTICLE_CURVE_SAMPLES; ++i)
        {
            emitter.sizeOverLife[i]  = static_cast<float>(i);
            emitter.speedOverLife[i] = 100.0f + static_cast<float>(i);
            emitter.colorOverLife[i] = { static_cast<float>(i), 0.0f, 0.0f, 1.0f };
        }
        return emitter;
    }

    HX::ParticleEmitterInstance MakeInstance()
    {
        HX::ParticleEmitterInstance instance;
        instance.WorldMatrix = HM::Matrix4x4::GetTranslation(5.0f, 6.0f, 7.0f);
        instance.SpawnCount  = 9;
        instance.Seed        = 1234;
        instance.DeltaTime   = 1.0f / 60.0f;
        return instance;
    }
}

TEST_CASE("Particle uniform - std140 offsets")
{
    CHECK(offsetof(ParticleEmitterUniform, EmitMatrix) == 0);
    CHECK(offsetof(ParticleEmitterUniform, DrawMatrix) == 64);
    CHECK(offsetof(ParticleEmitterUniform, StartColor) == 128);
    CHECK(offsetof(ParticleEmitterUniform, Gravity) == 144);
    CHECK(offsetof(ParticleEmitterUniform, Drag) == 156);
    CHECK(offsetof(ParticleEmitterUniform, BoxSize) == 160);
    CHECK(offsetof(ParticleEmitterUniform, LifetimeMin) == 176);
    CHECK(offsetof(ParticleEmitterUniform, SizeMin) == 192);
    CHECK(offsetof(ParticleEmitterUniform, ConeAngle) == 208);
    CHECK(offsetof(ParticleEmitterUniform, PlaneHeight) == 224);
    CHECK(offsetof(ParticleEmitterUniform, SlotFirst) == 240);
    CHECK(offsetof(ParticleEmitterUniform, Shape) == 256);
    CHECK(offsetof(ParticleEmitterUniform, SheetTilesX) == 272);
    CHECK(offsetof(ParticleEmitterUniform, SizeOverLife) == 288);
    CHECK(offsetof(ParticleEmitterUniform, SpeedOverLife) == 416);
    CHECK(offsetof(ParticleEmitterUniform, ColorOverLife) == 544);
}

TEST_CASE("Particle uniform - packs the emitter's settings, the instance and its slot")
{
    const ParticleEmitterUniform uniform = MakeParticleEmitterUniform(MakeEmitter(), MakeInstance(), { 100, 500 });
    CHECK(uniform.StartColor[1] == 0.5f);
    CHECK(uniform.Gravity[2] == -9.81f);
    CHECK(uniform.Drag == 0.1f);
    CHECK(uniform.BoxSize[2] == 3.0f);
    CHECK(uniform.Radius == 0.5f);
    CHECK(uniform.LifetimeMax == 2.0f);
    CHECK(uniform.SpeedMin == 3.0f);
    CHECK(uniform.SizeMax == 0.5f);
    CHECK(uniform.RotationMin == doctest::Approx(-std::numbers::pi_v<float>));
    CHECK(uniform.ConeAngle == doctest::Approx(std::numbers::pi_v<float> / 2.0f));
    CHECK(uniform.DeltaTime == doctest::Approx(1.0f / 60.0f));
    CHECK(uniform.SoftDistance == 0.3f);
    CHECK(uniform.SheetFps == 12.0f);
    CHECK(uniform.PlaneHeight == -1.0f);
    CHECK(uniform.Bounce == 0.5f);
    CHECK(uniform.LifetimeLoss == 0.25f);
    CHECK(uniform.SlotFirst == 100);
    CHECK(uniform.SlotCount == 500);
    CHECK(uniform.SpawnCount == 9);
    CHECK(uniform.Seed == 1234);
    CHECK(uniform.Shape == 2);
    CHECK(uniform.Blend == 1);
    CHECK(uniform.Space == 1);
    CHECK(uniform.Collision == 1);
    CHECK(uniform.SheetTilesX == 4);
    CHECK(uniform.SheetTilesY == 2);
    CHECK(uniform.EmitFromSurface == 1);
    // Four samples to a vec4, in order.
    CHECK(uniform.SizeOverLife[0][0] == 0.0f);
    CHECK(uniform.SizeOverLife[1][2] == 6.0f);
    CHECK(uniform.SizeOverLife[7][3] == 31.0f);
    CHECK(uniform.SpeedOverLife[2][1] == 109.0f);
    CHECK(uniform.ColorOverLife[31][0] == 31.0f);
    CHECK(uniform.ColorOverLife[31][3] == 1.0f);
}

TEST_CASE("Particle uniform - World space emits through the entity's matrix, Local space draws through it")
{
    HedgehogEngine::ParticleEmitterView emitter  = MakeEmitter();
    const HX::ParticleEmitterInstance   instance = MakeInstance();

    emitter.space                      = 1; // World
    const ParticleEmitterUniform world = MakeParticleEmitterUniform(emitter, instance, {});
    CHECK(world.EmitMatrix == instance.WorldMatrix);
    CHECK(world.DrawMatrix == HM::Matrix4x4::GetIdentity());

    emitter.space                      = 0; // Local
    const ParticleEmitterUniform local = MakeParticleEmitterUniform(emitter, instance, {});
    CHECK(local.EmitMatrix == HM::Matrix4x4::GetIdentity());
    CHECK(local.DrawMatrix == instance.WorldMatrix);
}

TEST_CASE("Particle uniform - a non-finite number becomes 0")
{
    const float nan  = std::numeric_limits<float>::quiet_NaN();
    const float inf  = std::numeric_limits<float>::infinity();
    HedgehogEngine::ParticleEmitterView emitter = MakeEmitter();
    emitter.drag             = nan;
    emitter.gravity          = { inf, 1.0f, -inf };
    emitter.angle            = nan;
    emitter.sizeOverLife[3]  = inf;
    emitter.colorOverLife[5] = { nan, 1.0f, 1.0f, 1.0f };
    HX::ParticleEmitterInstance instance = MakeInstance();
    instance.DeltaTime                   = nan;

    const ParticleEmitterUniform uniform = MakeParticleEmitterUniform(emitter, instance, {});
    CHECK(uniform.Drag == 0.0f);
    CHECK(uniform.Gravity[0] == 0.0f);
    CHECK(uniform.Gravity[1] == 1.0f);
    CHECK(uniform.Gravity[2] == 0.0f);
    CHECK(uniform.ConeAngle == 0.0f);
    CHECK(uniform.DeltaTime == 0.0f);
    CHECK(uniform.SizeOverLife[0][3] == 0.0f);
    CHECK(uniform.ColorOverLife[5][0] == 0.0f);
}
