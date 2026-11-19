#pragma once

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <cstdint>
#include <vector>

namespace HedgehogEngine
{
    class ResourceCatalog;
    struct ParticleEmitter;
    struct ParticleEmitterClock;

    // One emitter of one particle system for one frame: what it spawns this frame, for extraction.
    struct ParticleFrameEmitter
    {
        ECS::Entity Entity       = ECS::INVALID_ENTITY;
        uint32_t    EffectIndex  = 0;
        uint32_t    EmitterIndex = 0;
        uint32_t    SpawnCount   = 0;
        uint32_t    Seed         = 0;     // differs per entity, emitter and frame, the same on every run
        float       DeltaTime    = 0.0f;  // the frame's scaled time while playing, else 0
        bool        Playing      = false;
        uint32_t    Epoch        = 0;     // the component's Epoch
    };

    // The particle systems (ParticleSystemComponent + TransformComponent). In the Late phase it
    // resolves each component's effect through the ResourceCatalog service's ParticleEffectContainer
    // (reloading changed files) and, while Playing, advances every emitter by the frame's scaled time,
    // counting the particles to spawn: its rate's whole particles (the fraction carried to the next
    // frame) and every burst whose time falls in the frame, until its duration unless it loops (a
    // loop starts the run again, its bursts with it). Edit and Paused spawn nothing, but for the one
    // instance the editor previews (SetPreview), which runs in Edit by the frame's real time once
    // Play starts it. The frame of a scene switch or save load during Play (SceneLoadedEvent,
    // GameLoadedEvent) starts the loaded instances that are not playing, as Play start does. An
    // effect reloaded while playing starts its instances again. Nothing without the catalog service.
    class ParticleSystem : public ECS::System
    {
    public:
        HEDGEHOG_ENGINE_API void OnRegister(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnUnregister(ECS::ECS& ecs) override;
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::SystemPhase GetPhase() const override;
        HEDGEHOG_ENGINE_API void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx) override;
        HEDGEHOG_ENGINE_API void OnPlayStart(ECS::ECS& ecs) override;
        HEDGEHOG_ENGINE_API void OnPlayStop(ECS::ECS& ecs) override;

        // Every enabled instance with an effect, one entry per emitter, as of the last frame (by
        // entity id, then emitter). Kept until the next frame; its storage is reused.
        [[nodiscard]] const std::vector<ParticleFrameEmitter>& GetFrameEmitters() const { return m_FrameEmitters; }

        // Starts the entity's particle system from the beginning of its run.
        HEDGEHOG_ENGINE_API void Play(ECS::ECS& ecs, ECS::Entity entity);
        // Stops spawning; with clear, the particles alive go too (a new Epoch).
        HEDGEHOG_ENGINE_API void Stop(ECS::ECS& ecs, ECS::Entity entity, bool clear);
        // Adds count particles to every emitter's next frame, playing or not.
        HEDGEHOG_ENGINE_API void Emit(ECS::ECS& ecs, ECS::Entity entity, uint32_t count);
        // Whether it plays: started and not past the duration of every emitter that does not loop.
        [[nodiscard]] HEDGEHOG_ENGINE_API bool IsPlaying(const ECS::ECS& ecs, ECS::Entity entity) const;

        // The entity whose particles run in Edit mode (Play, Stop and Emit drive it as in Play), or
        // INVALID_ENTITY for none. The one previewed before stops, its particles cleared; Play mode
        // ends the preview. Only runtime fields change, so a scene saves the same.
        HEDGEHOG_ENGINE_API void SetPreview(ECS::ECS& ecs, ECS::Entity entity);
        [[nodiscard]] ECS::Entity GetPreview() const { return m_Preview; }

        // How many particles the emitter spawns from time t0 over dt seconds into its run, moving
        // clock on; public so tests can drive one emitter alone.
        [[nodiscard]] HEDGEHOG_ENGINE_API static uint32_t Advance(const ParticleEmitter& emitter,
                                                                  ParticleEmitterClock& clock, double dt);

    private:
        void Resolve(ECS::ECS& ecs, ECS::Entity entity);
        void StartPlayOnStart(ECS::ECS& ecs);

        ResourceCatalog*                  m_Catalog           = nullptr;
        EventBus*                         m_Bus               = nullptr;
        SubscriptionId                    m_SceneSubscription = SubscriptionId::Invalid;
        SubscriptionId                    m_GameSubscription  = SubscriptionId::Invalid;
        bool                              m_StartPending      = false; // a world was loaded during Play
        uint32_t                          m_Frame             = 0; // frames played since Play began, or previewed
        ECS::Entity                       m_Preview           = ECS::INVALID_ENTITY;
        std::vector<ParticleFrameEmitter> m_FrameEmitters;
    };
}
