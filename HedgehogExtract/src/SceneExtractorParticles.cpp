#include "api/SceneExtractor.hpp"
#include "Profiler/api/Profiler.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"

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
}
