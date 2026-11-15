#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    // One emitter's run, kept by the ParticleSystem: seconds into the current run (wrapped when the
    // emitter loops) and the fraction of a particle its rate has owed but not yet spawned.
    struct ParticleEmitterClock
    {
        double Time      = 0.0;
        double RateCarry = 0.0;
        bool   Finished  = false; // a run that does not loop has reached its duration
    };

    // A particle effect (a .particle file under assets://, ParticleEffect.hpp) placed at its entity.
    // While Enabled it plays in Play mode, from the start of Play when PlayOnStart; Seed makes one
    // instance's random values differ from another's.
HH_BEGIN_COMPONENT(ParticleSystemComponent)
    HH_PROP_NAMED(bool,        Enabled,     "Enabled",     true,          None)
    HH_PROP_NAMED(std::string, Effect,      "Effect",      std::string{}, AssetRef)
    HH_PROP_NAMED(bool,        PlayOnStart, "PlayOnStart", true,          None)
    HH_PROP_NAMED(uint32_t,    Seed,        "Seed",        0u,            None)

    // Runtime only, never written. The effect's index in the ParticleEffectContainer (-1 for none,
    // or a file that does not load), the Effect path and effect version it was resolved from, so a
    // changed path or a reloaded effect is taken again; whether it plays; one clock per emitter;
    // particles Emit asked for, spawned on the next frame; and Epoch, bumped whenever the instance
    // starts again or is cleared, so whatever simulates it starts afresh.
    int32_t                           EffectIndex   = -1;
    std::string                       ResolvedEffect;
    uint32_t                          EffectVersion = 0;
    bool                              Playing       = false;
    std::vector<ParticleEmitterClock> Clocks;
    uint32_t                          PendingEmit   = 0;
    uint32_t                          Epoch         = 0;
HH_END_COMPONENT(ParticleSystemComponent)
}
