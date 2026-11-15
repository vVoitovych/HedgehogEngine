#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include <cstdint>
#include <string>

namespace HedgehogEngine
{
    // A particle effect (a .particle file under assets://, ParticleEffect.hpp) placed at its entity.
    // While Enabled it plays in Play mode, from the start of Play when PlayOnStart; Seed makes one
    // instance's random values differ from another's.
HH_BEGIN_COMPONENT(ParticleSystemComponent)
    HH_PROP_NAMED(bool,        Enabled,     "Enabled",     true,          None)
    HH_PROP_NAMED(std::string, Effect,      "Effect",      std::string{}, AssetRef)
    HH_PROP_NAMED(bool,        PlayOnStart, "PlayOnStart", true,          None)
    HH_PROP_NAMED(uint32_t,    Seed,        "Seed",        0u,            None)
HH_END_COMPONENT(ParticleSystemComponent)
}
