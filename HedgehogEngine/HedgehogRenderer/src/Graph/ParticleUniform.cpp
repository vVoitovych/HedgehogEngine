#include "HedgehogRenderer/Graph/ParticleUniform.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"

#include <cmath>
#include <numbers>

namespace Renderer
{
    namespace
    {
        constexpr uint8_t LOCAL_SPACE = 0; // HedgehogEngine's ParticleSpace::Local

        float Finite(float value)
        {
            return std::isfinite(value) ? value : 0.0f;
        }

        float Radians(float degrees)
        {
            return Finite(degrees) * std::numbers::pi_v<float> / 180.0f;
        }

        void Copy(float* out, std::initializer_list<float> values)
        {
            for (const float value : values)
                *out++ = Finite(value);
        }
    }

    ParticleEmitterUniform MakeParticleEmitterUniform(const HedgehogEngine::ParticleEmitterView& emitter,
                                                      const HX::ParticleEmitterInstance& instance, ParticleSlot slot)
    {
        ParticleEmitterUniform uniform;
        const bool local   = emitter.space == LOCAL_SPACE;
        uniform.EmitMatrix = local ? HM::Matrix4x4::GetIdentity() : instance.WorldMatrix;
        uniform.DrawMatrix = local ? instance.WorldMatrix : HM::Matrix4x4::GetIdentity();

        Copy(uniform.StartColor, { emitter.startColor.x(), emitter.startColor.y(), emitter.startColor.z(), emitter.startColor.w() });
        Copy(uniform.Gravity, { emitter.gravity.x(), emitter.gravity.y(), emitter.gravity.z() });
        Copy(uniform.BoxSize, { emitter.boxSize.x(), emitter.boxSize.y(), emitter.boxSize.z() });
        uniform.Drag         = Finite(emitter.drag);
        uniform.Radius       = Finite(emitter.radius);
        uniform.LifetimeMin  = Finite(emitter.lifetimeMin);
        uniform.LifetimeMax  = Finite(emitter.lifetimeMax);
        uniform.SpeedMin     = Finite(emitter.speedMin);
        uniform.SpeedMax     = Finite(emitter.speedMax);
        uniform.SizeMin      = Finite(emitter.sizeMin);
        uniform.SizeMax      = Finite(emitter.sizeMax);
        uniform.RotationMin  = Radians(emitter.rotationMin);
        uniform.RotationMax  = Radians(emitter.rotationMax);
        uniform.ConeAngle    = Radians(emitter.angle);
        uniform.DeltaTime    = Finite(instance.DeltaTime);
        uniform.SoftDistance = Finite(emitter.softDistance);
        uniform.SheetFps     = Finite(emitter.sheetFps);
        uniform.PlaneHeight  = Finite(emitter.planeHeight);
        uniform.Bounce       = Finite(emitter.bounce);
        uniform.LifetimeLoss = Finite(emitter.lifetimeLoss);

        uniform.SlotFirst       = slot.First;
        uniform.SlotCount       = slot.Count;
        uniform.SpawnCount      = instance.SpawnCount;
        uniform.Seed            = instance.Seed;
        uniform.Shape           = emitter.shape;
        uniform.Blend           = emitter.blend;
        uniform.Space           = emitter.space;
        uniform.Collision       = emitter.collisionMode;
        uniform.SheetTilesX     = emitter.sheetTilesX;
        uniform.SheetTilesY     = emitter.sheetTilesY;
        uniform.EmitFromSurface = emitter.emitFromSurface ? 1u : 0u;

        for (uint32_t i = 0; i < HedgehogEngine::PARTICLE_CURVE_SAMPLES; ++i)
        {
            uniform.SizeOverLife[i / 4][i % 4]  = Finite(emitter.sizeOverLife[i]);
            uniform.SpeedOverLife[i / 4][i % 4] = Finite(emitter.speedOverLife[i]);
            const HM::Vector4& color             = emitter.colorOverLife[i];
            Copy(uniform.ColorOverLife[i], { color.x(), color.y(), color.z(), color.w() });
        }
        return uniform;
    }
}
