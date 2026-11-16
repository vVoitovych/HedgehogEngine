#include "api/SceneExtractor.hpp"
#include "Profiler/api/Profiler.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <cmath>
#include <numbers>

namespace
{
    constexpr int SHAPE_CIRCLE_SEGMENTS = 24;
    constexpr float POINT_CROSS_HALF = 0.1f;
}

namespace HX
{
    void SceneExtractor::ExtractParticles(const ECS::ECS& ecs, const HedgehogEngine::ParticleSystem& particleSystem,
                                          RenderScene& outScene) const
    {
        HH_PROFILE_ZONE("Extract: particles");
        for (const HedgehogEngine::ParticleFrameEmitter& emitter : particleSystem.GetFrameEmitters())
        {
            if (!ecs.IsAlive(emitter.Entity) || !ecs.HasComponent<HedgehogEngine::TransformComponent>(emitter.Entity))
                continue;
            ParticleEmitterInstance instance;
            instance.InstanceId   = static_cast<uint64_t>(ecs.GetGeneration(emitter.Entity)) << 32 | static_cast<uint64_t>(emitter.Entity);
            instance.Epoch        = emitter.Epoch;
            instance.EffectIndex  = emitter.EffectIndex;
            instance.EmitterIndex = emitter.EmitterIndex;
            instance.WorldMatrix  = ecs.GetComponent<HedgehogEngine::TransformComponent>(emitter.Entity).ObjMatrix;
            instance.SpawnCount   = emitter.SpawnCount;
            instance.Seed         = emitter.Seed;
            instance.DeltaTime    = emitter.DeltaTime;
            instance.Playing      = emitter.Playing;
            outScene.ParticleEmitters.push_back(instance);
        }
    }

    void SceneExtractor::ExtractParticleShapes(const ECS::ECS& ecs, const HedgehogEngine::ParticleEffectContainer& effects,
                                               ECS::Entity entity, RenderScene& outScene) const
    {
        using HedgehogEngine::ParticleShape;
        if (!ecs.IsAlive(entity) || !ecs.HasComponent<HedgehogEngine::ParticleSystemComponent>(entity)
            || !ecs.HasComponent<HedgehogEngine::TransformComponent>(entity))
            return;
        const int32_t index = ecs.GetComponent<HedgehogEngine::ParticleSystemComponent>(entity).EffectIndex;
        const HedgehogEngine::ParticleEffect* effect = index >= 0 ? effects.GetEffect(static_cast<size_t>(index)) : nullptr;
        if (!effect)
            return;

        const HM::Matrix4x4& world = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity).ObjMatrix;
        const auto line = [&](const HM::Vector3& a, const HM::Vector3& b)
        {
            const HM::Vector4 wa = world * HM::Vector4(a.x(), a.y(), a.z(), 1.0f);
            const HM::Vector4 wb = world * HM::Vector4(b.x(), b.y(), b.z(), 1.0f);
            outScene.DebugLines.push_back({ HM::Vector3(wa.x(), wa.y(), wa.z()), PARTICLE_SHAPE_COLOR });
            outScene.DebugLines.push_back({ HM::Vector3(wb.x(), wb.y(), wb.z()), PARTICLE_SHAPE_COLOR });
        };
        // A circle about center in the plane of the unit axes u and v.
        const auto circle = [&](const HM::Vector3& center, const HM::Vector3& u, const HM::Vector3& v, float radius)
        {
            const auto at = [&](int i)
            {
                const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / SHAPE_CIRCLE_SEGMENTS;
                return center + u * (std::cos(angle) * radius) + v * (std::sin(angle) * radius);
            };
            for (int i = 0; i < SHAPE_CIRCLE_SEGMENTS; ++i)
                line(at(i), at(i + 1));
        };
        const HM::Vector3 o(0.0f, 0.0f, 0.0f), x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f), z(0.0f, 0.0f, 1.0f);

        for (const HedgehogEngine::ParticleEmitter& emitter : effect->Emitters)
        {
            switch (emitter.Shape)
            {
            case ParticleShape::Point:
                for (const HM::Vector3& axis : { x, y, z })
                    line(axis * -POINT_CROSS_HALF, axis * POINT_CROSS_HALF);
                break;
            case ParticleShape::Sphere:
                circle(o, x, y, emitter.Radius);
                circle(o, y, z, emitter.Radius);
                circle(o, z, x, emitter.Radius);
                break;
            case ParticleShape::Cone:
            {
                const float top = emitter.Radius + std::tan(emitter.Angle * std::numbers::pi_v<float> / 180.0f);
                circle(o, x, y, emitter.Radius);
                circle(z, x, y, top);
                for (const HM::Vector3& side : { x, -x, y, -y })
                    line(side * emitter.Radius, z + side * top);
                break;
            }
            case ParticleShape::Box:
            {
                const HM::Vector3 half = emitter.BoxSize * 0.5f;
                // Corner k has bit i set for +half[i]; an edge joins corners one bit apart.
                const auto corner = [&](int k)
                {
                    return HM::Vector3((k & 1) ? half.x() : -half.x(), (k & 2) ? half.y() : -half.y(),
                                       (k & 4) ? half.z() : -half.z());
                };
                for (int k = 0; k < 8; ++k)
                    for (int bit = 1; bit < 8; bit <<= 1)
                        if ((k & bit) == 0)
                            line(corner(k), corner(k | bit));
                break;
            }
            }
        }
    }
}
