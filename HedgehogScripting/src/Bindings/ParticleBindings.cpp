#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::ParticleSystemComponent;
        using ParticleSystemRef = ScriptComponentRef<ParticleSystemComponent>;

        // A whole number from 0 to UINT32_MAX, or a script error naming what it is.
        uint32_t RequireCount(double value, const char* what)
        {
            if (!std::isfinite(value) || value < 0.0 || std::floor(value) != value
                || value > static_cast<double>(std::numeric_limits<uint32_t>::max()))
                throw std::runtime_error(std::string(what) + " must be a whole number of at least 0");
            return static_cast<uint32_t>(value);
        }

        // Particles play only in Play mode (a script's top level also runs when the editor
        // describes it): outside it a call does nothing and the first one logs why.
        struct PlayGuard
        {
            HedgehogEngine::EngineContext& Context;
            bool                           Warned = false;

            bool Allows(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Particles play only in Play mode;", std::string(call), "did nothing.");
                Warned = true;
                return false;
            }
        };
    }

    void RegisterParticles(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        ECS::ECS&                       ecs    = context.GetECS();
        HedgehogEngine::ParticleSystem& system = *context.GetParticleSystem();
        auto                            guard  = std::make_shared<PlayGuard>(PlayGuard{ context });
        lua.new_usertype<ParticleSystemRef>(
            "ParticleSystem", sol::no_constructor,
            // play(): the effect from the start of its run.
            "play", [&ecs, &system, guard](const ParticleSystemRef& ref)
            {
                (void)Resolve(ecs, ref);
                if (guard->Allows("ParticleSystem:play()"))
                    system.Play(ecs, ref.Entity.Id);
            },
            // stop(clear?): no more spawning; with clear, the particles alive go too.
            "stop", [&ecs, &system, guard](const ParticleSystemRef& ref, sol::optional<bool> clear)
            {
                (void)Resolve(ecs, ref);
                if (guard->Allows("ParticleSystem:stop()"))
                    system.Stop(ecs, ref.Entity.Id, clear.value_or(false));
            },
            // emit(count): count more particles from every emitter on the next frame, playing or not.
            "emit", [&ecs, &system, guard](const ParticleSystemRef& ref, double count)
            {
                (void)Resolve(ecs, ref);
                const uint32_t particles = RequireCount(count, "a particle count");
                if (guard->Allows("ParticleSystem:emit()"))
                    system.Emit(ecs, ref.Entity.Id, particles);
            },
            // False once stopped, or once every emitter that does not loop has run its course.
            "isPlaying", [&ecs, &system](const ParticleSystemRef& ref)
            {
                (void)Resolve(ecs, ref);
                return system.IsPlaying(ecs, ref.Entity.Id);
            },
            // The .particle under assets://, prefix optional; a new one starts the system again.
            "effect", sol::property(
                [&ecs](const ParticleSystemRef& ref) { return Resolve(ecs, ref).Effect; },
                [&ecs](const ParticleSystemRef& ref, const std::string& path)
                { Resolve(ecs, ref).Effect = ToAssetPath(path, "a particle effect", "particle effects"); }),
            "enabled", sol::property(
                [&ecs](const ParticleSystemRef& ref) { return Resolve(ecs, ref).Enabled; },
                [&ecs](const ParticleSystemRef& ref, bool enabled) { Resolve(ecs, ref).Enabled = enabled; }),
            "playOnStart", sol::property(
                [&ecs](const ParticleSystemRef& ref) { return Resolve(ecs, ref).PlayOnStart; },
                [&ecs](const ParticleSystemRef& ref, bool playOnStart) { Resolve(ecs, ref).PlayOnStart = playOnStart; }),
            "seed", sol::property(
                [&ecs](const ParticleSystemRef& ref) { return Resolve(ecs, ref).Seed; },
                [&ecs](const ParticleSystemRef& ref, double seed) { Resolve(ecs, ref).Seed = RequireCount(seed, "a seed"); }),
            sol::meta_function::to_string, ToText<ParticleSystemComponent>("ParticleSystem"));
    }
}
