#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

namespace HedgehogEngine
{
    // Hears the scene's sounds from its entity's transform (looking down -Z with +Y up). The first
    // active one, by entity id, is the listener; with none, the scene's camera is.
HH_BEGIN_COMPONENT(AudioListenerComponent)
    HH_PROP_NAMED(bool, IsActive, "Active", true, None)
HH_END_COMPONENT(AudioListenerComponent)
}
