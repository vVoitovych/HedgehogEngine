#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"

#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace HedgehogEngine
{
    namespace
    {
        // ponytail: a loop shorter than the frame wraps at most this many times a frame; past it the
        // rest of the frame is dropped, as no effect needs a run shorter than a few milliseconds.
        constexpr int MAX_WRAPS_PER_FRAME = 1000;

        // The bursts of emitter whose firing times fall in [from, to) seconds into its run.
        uint64_t CountBursts(const ParticleEmitter& emitter, double from, double to)
        {
            uint64_t count = 0;
            for (const ParticleBurst& burst : emitter.Bursts)
            {
                const double time = burst.Time;
                if (burst.Cycles == 1 || burst.Interval <= 0.0f)
                {
                    if (time >= from && time < to)
                        count += burst.Count;
                    continue;
                }
                // Firing k at time + k * interval, for k from 0 to Cycles - 1 (forever with 0).
                const double interval = burst.Interval;
                double       first    = std::max(0.0, std::ceil((from - time) / interval));
                double       end      = std::max(0.0, std::ceil((to - time) / interval)); // exclusive
                if (burst.Cycles != 0)
                    end = std::min(end, static_cast<double>(burst.Cycles));
                if (end > first)
                    count += static_cast<uint64_t>(end - first) * burst.Count;
            }
            return count;
        }

        // A well-mixed 32-bit value of the inputs (MurmurHash3's finalizer over their combination).
        uint32_t MixSeed(uint32_t seed, uint32_t entity, uint32_t emitter, uint32_t frame)
        {
            uint32_t h = seed * 0x9E3779B1u ^ entity * 0x85EBCA77u ^ emitter * 0xC2B2AE3Du ^ frame * 0x27D4EB2Fu;
            h ^= h >> 16;
            h *= 0x85EBCA6Bu;
            h ^= h >> 13;
            h *= 0xC2B2AE35u;
            h ^= h >> 16;
            return h;
        }

        void Restart(ParticleSystemComponent& particles)
        {
            particles.Clocks.clear();
            particles.PendingEmit = 0;
            ++particles.Epoch;
        }
    }

    void ParticleSystem::OnRegister(ECS::ECS& ecs)
    {
        m_Catalog = ecs.GetServices().Find<ResourceCatalog>();
        m_Bus     = ecs.GetServices().Find<EventBus>();
        if (m_Bus)
        {
            // A loaded world's instances are not playing yet; they start in this frame's OnFrame, once its transforms are applied.
            m_SceneSubscription = m_Bus->Subscribe<SceneLoadedEvent>([this](const SceneLoadedEvent&)
            {
                m_StartPending = true;
            });
            m_GameSubscription = m_Bus->Subscribe<GameLoadedEvent>([this](const GameLoadedEvent&)
            {
                m_StartPending = true;
            });
        }
    }

    void ParticleSystem::OnUnregister(ECS::ECS& /*ecs*/)
    {
        if (m_Bus)
        {
            m_Bus->Unsubscribe(m_SceneSubscription);
            m_Bus->Unsubscribe(m_GameSubscription);
        }
        m_Bus               = nullptr;
        m_SceneSubscription = SubscriptionId::Invalid;
        m_GameSubscription  = SubscriptionId::Invalid;
        m_Catalog           = nullptr;
    }

    ECS::SystemPhase ParticleSystem::GetPhase() const
    {
        return ECS::SystemPhase::Late;
    }

    uint32_t ParticleSystem::Advance(const ParticleEmitter& emitter, ParticleEmitterClock& clock, double dt)
    {
        if (clock.Finished || !(dt > 0.0))
            return 0;
        const bool   hasDuration = emitter.Duration > 0.0f;
        const double duration    = hasDuration ? emitter.Duration : std::numeric_limits<double>::infinity();
        if (!hasDuration && !emitter.Loop)
        {
            clock.Finished = true; // a run of no length that does not loop spawns nothing
            return 0;
        }

        uint64_t count     = 0;
        double   remaining = dt;
        for (int wraps = 0; remaining > 0.0 && wraps <= MAX_WRAPS_PER_FRAME; ++wraps)
        {
            const double from = clock.Time;
            const double span = std::min(remaining, duration - from);
            const double to   = from + span;

            clock.RateCarry += static_cast<double>(emitter.Rate) * span;
            // A hair of slack, so a second's worth of tenths adds up to one particle, not 0.9999.
            const double whole = std::floor(clock.RateCarry + 1e-9);
            clock.RateCarry    = std::max(0.0, clock.RateCarry - whole);
            count += static_cast<uint64_t>(whole) + CountBursts(emitter, from, to);

            clock.Time = to;
            remaining -= span;
            if (clock.Time < duration)
                break;
            if (!emitter.Loop)
            {
                clock.Finished = true;
                break;
            }
            clock.Time = 0.0; // the run starts again, its bursts with it
        }
        return static_cast<uint32_t>(std::min<uint64_t>(count, std::numeric_limits<uint32_t>::max()));
    }

    void ParticleSystem::Resolve(ECS::ECS& ecs, ECS::Entity entity)
    {
        ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
        ParticleEffectContainer& effects   = m_Catalog->GetParticleEffectContainer();
        if (particles.ResolvedEffect != particles.Effect)
        {
            const std::optional<size_t> index = effects.FindOrLoad(particles.Effect);
            particles.EffectIndex             = index ? static_cast<int32_t>(*index) : -1;
            particles.ResolvedEffect          = particles.Effect;
            particles.EffectVersion           = index ? effects.GetVersion(*index) : 0;
            Restart(particles);
            return;
        }
        // A reloaded effect starts its instances again.
        if (particles.EffectIndex >= 0)
        {
            const uint32_t version = effects.GetVersion(static_cast<size_t>(particles.EffectIndex));
            if (version != particles.EffectVersion)
            {
                particles.EffectVersion = version;
                Restart(particles);
            }
        }
    }

    void ParticleSystem::OnFrame(ECS::ECS& ecs, const ECS::FrameContext& ctx)
    {
        if (m_StartPending)
        {
            m_StartPending = false;
            if (ctx.Mode == ECS::PlayMode::Playing)
            {
                StartPlayOnStart(ecs);
            }
        }
        m_FrameEmitters.clear();
        if (!m_Catalog)
            return;
        ParticleEffectContainer& effects = m_Catalog->GetParticleEffectContainer();
        effects.ReloadChanged(std::chrono::steady_clock::now());

        const bool  playing = ctx.Mode == ECS::PlayMode::Playing;
        const float dt      = playing ? ctx.ScaledDeltaTime : 0.0f;
        const bool  preview = ctx.Mode == ECS::PlayMode::Edit && m_Preview != ECS::INVALID_ENTITY;
        if (playing || preview)
            ++m_Frame;

        for (const ECS::Entity entity : m_Entities)
        {
            Resolve(ecs, entity);
            ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
            if (!particles.Enabled || particles.EffectIndex < 0)
                continue;
            const ParticleEffect* effect = effects.GetEffect(static_cast<size_t>(particles.EffectIndex));
            if (!effect)
                continue;
            particles.Clocks.resize(effect->Emitters.size());

            // The previewed instance runs in Edit as it would in Play, by the frame's real time.
            const bool  previewed = preview && entity == m_Preview;
            const bool  active    = playing || previewed;
            const float entityDt  = previewed ? std::max(0.0f, ctx.DeltaTime) : dt;
            const bool  running   = active && particles.Playing;
            bool        finished  = true;
            for (size_t index = 0; index < effect->Emitters.size(); ++index)
            {
                const ParticleEmitter& emitter = effect->Emitters[index];
                ParticleEmitterClock&  clock   = particles.Clocks[index];

                uint64_t spawn = running ? Advance(emitter, clock, entityDt) : 0;
                if (active)
                    spawn += particles.PendingEmit;
                finished = finished && clock.Finished;

                ParticleFrameEmitter entry;
                entry.Entity       = entity;
                entry.EffectIndex  = static_cast<uint32_t>(particles.EffectIndex);
                entry.EmitterIndex = static_cast<uint32_t>(index);
                entry.SpawnCount   = static_cast<uint32_t>(std::min<uint64_t>(spawn, std::numeric_limits<uint32_t>::max()));
                entry.Seed         = MixSeed(particles.Seed, static_cast<uint32_t>(entity), static_cast<uint32_t>(index), m_Frame);
                entry.DeltaTime    = entityDt;
                entry.Playing      = active;
                entry.Epoch        = particles.Epoch;
                m_FrameEmitters.push_back(entry);
            }
            if (active)
                particles.PendingEmit = 0;
            if (running && finished)
                particles.Playing = false; // every emitter has run its course
        }

        // By entity, then emitter, whatever order the entities joined the system in.
        std::sort(m_FrameEmitters.begin(), m_FrameEmitters.end(),
                  [](const ParticleFrameEmitter& a, const ParticleFrameEmitter& b)
                  { return a.Entity != b.Entity ? a.Entity < b.Entity : a.EmitterIndex < b.EmitterIndex; });
    }

    void ParticleSystem::OnPlayStart(ECS::ECS& ecs)
    {
        m_Frame   = 0;
        m_Preview = ECS::INVALID_ENTITY;
        for (const ECS::Entity entity : m_Entities)
        {
            ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
            Restart(particles);
            particles.Playing = false;
        }
        m_StartPending = false;
        StartPlayOnStart(ecs);
    }

    void ParticleSystem::StartPlayOnStart(ECS::ECS& ecs)
    {
        for (const ECS::Entity entity : m_Entities)
        {
            ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
            if (!particles.Playing && particles.Enabled && particles.PlayOnStart)
            {
                Restart(particles);
                particles.Playing = true;
            }
        }
    }

    void ParticleSystem::OnPlayStop(ECS::ECS& ecs)
    {
        m_StartPending = false;
        for (const ECS::Entity entity : m_Entities)
        {
            ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
            Restart(particles);
            particles.Playing = false;
        }
    }

    void ParticleSystem::Play(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (!ecs.HasComponent<ParticleSystemComponent>(entity))
            return;
        ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
        Restart(particles);
        particles.Playing = true;
    }

    void ParticleSystem::Stop(ECS::ECS& ecs, ECS::Entity entity, bool clear)
    {
        if (!ecs.HasComponent<ParticleSystemComponent>(entity))
            return;
        ParticleSystemComponent& particles = ecs.GetComponent<ParticleSystemComponent>(entity);
        particles.Playing = false;
        if (clear)
            ++particles.Epoch;
    }

    void ParticleSystem::Emit(ECS::ECS& ecs, ECS::Entity entity, uint32_t count)
    {
        if (!ecs.HasComponent<ParticleSystemComponent>(entity))
            return;
        uint32_t& pending = ecs.GetComponent<ParticleSystemComponent>(entity).PendingEmit;
        pending           = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{ pending } + count, std::numeric_limits<uint32_t>::max()));
    }

    void ParticleSystem::SetPreview(ECS::ECS& ecs, ECS::Entity entity)
    {
        if (entity == m_Preview)
            return;
        if (m_Preview != ECS::INVALID_ENTITY && ecs.IsAlive(m_Preview))
            Stop(ecs, m_Preview, true);
        m_Preview = entity;
    }

    bool ParticleSystem::IsPlaying(const ECS::ECS& ecs, ECS::Entity entity) const
    {
        return ecs.HasComponent<ParticleSystemComponent>(entity) && ecs.GetComponent<ParticleSystemComponent>(entity).Playing;
    }
}
