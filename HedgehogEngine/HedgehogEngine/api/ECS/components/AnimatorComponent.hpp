#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    // Plays one of the clips of the entity's skinned mesh. The reflected fields are saved with the
    // scene; the rest is AnimationSystem's runtime state, rebuilt every Play.
HH_BEGIN_COMPONENT(AnimatorComponent)
    HH_PROP_NAMED(std::string, Clip,          "Clip",          std::string{}, None)
    HH_PROP_NAMED(float,       Speed,         "Speed",         1.0f,          None)
    HH_PROP_NAMED(bool,        Loop,          "Loop",          true,          None)
    HH_PROP_NAMED(bool,        PlayOnStart,   "PlayOnStart",   true,          None)
    HH_PROP_NAMED(float,       CrossfadeTime, "CrossfadeTime", 0.2f,          None)

    bool        Started      = false;        // runtime: Play has initialized this animator
    bool        Playing      = false;        // runtime: the current clip advances
    std::string CurrentClip;                 // runtime: the clip being played (Clip changing starts a new one)
    float       Time         = 0.0f;         // runtime: seconds into the current clip
    std::string PreviousClip;                // runtime: the clip faded out of, empty when not fading
    float       PreviousTime = 0.0f;         // runtime
    float       FadeElapsed  = 0.0f;         // runtime
    std::string WarnedClip;                  // runtime: the unknown clip name already warned about
    std::optional<float> PreviewTime;        // runtime: in Edit mode, the time to show (else the bind pose)
    std::vector<HM::Matrix4x4> Palette;      // runtime: one skinning matrix per joint
HH_END_COMPONENT(AnimatorComponent)
}
