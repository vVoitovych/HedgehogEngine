#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogAudio/api/SoundHandle.hpp"

#include <string>

namespace HedgehogEngine
{
    // Plays a clip (a WAV, FLAC or MP3 under assets://) from its entity in Play mode: started on Play
    // when PlayOnStart, placed at the entity's transform when Spatial, and stopped by Stop.
HH_BEGIN_COMPONENT(AudioSourceComponent)
    HH_PROP_NAMED(std::string, Clip,        "Clip",        std::string{}, AssetRef)
    HH_PROP_NAMED_SLIDER(float, Volume,     "Volume",      1.0f, 0.0f, 1.0f)
    HH_PROP_NAMED(float,       Pitch,       "Pitch",       1.0f,          None)
    HH_PROP_NAMED(bool,        Loop,        "Loop",        false,         None)
    HH_PROP_NAMED(bool,        PlayOnStart, "PlayOnStart", true,          None)
    HH_PROP_NAMED(bool,        Spatial,     "Spatial",     true,          None)
    HH_PROP_NAMED(float,       MinDistance, "MinDistance", 1.0f,          None)
    HH_PROP_NAMED(float,       MaxDistance, "MaxDistance", 100.0f,        None)

    HA::SoundHandle Sound; // runtime: the sound playing, invalid when none
HH_END_COMPONENT(AudioSourceComponent)
}
