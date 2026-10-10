#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogAnimation/api/BlendStack.hpp"
#include "HedgehogAnimation/api/StateMachine.hpp"
#include "HedgehogMath/api/Matrix.hpp"

#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    // An animator's controller as AnimationSystem runs it: runtime only.
    struct AnimatorControllerRuntime
    {
        std::string Path;            // the Controller text the slot was found for
        int32_t     Slot    = -1;    // AnimationSystem's controller cache slot; -1 for none
        uint32_t    Version = 0;     // the slot's version Machine and Durations were made for

        HedgehogAnimation::StateMachineInstance Machine;
        bool Ready   = false;        // Machine holds the controller's parameters
        bool Running = false;        // a state is playing

        std::optional<uint64_t> Mesh;     // the mesh Durations were resolved for
        std::vector<float>      Durations; // per state, its clip's length
        bool                    Usable = false; // the mesh has every state's clip

        std::string RequestedState;  // AnimationSystem::Play's state, for the next frame
    };

    // Plays the clips of the entity's skinned mesh: Clip, or the states of Controller, an
    // animator controller file (.animctrl) whose transitions pick them. The reflected fields are
    // saved with the scene; the rest is AnimationSystem's runtime state, rebuilt every Play.
HH_BEGIN_COMPONENT(AnimatorComponent)
    HH_PROP_NAMED(std::string, Clip,          "Clip",          std::string{}, None)
    HH_PROP_NAMED(float,       Speed,         "Speed",         1.0f,          None)
    HH_PROP_NAMED(bool,        Loop,          "Loop",          true,          None)
    HH_PROP_NAMED(bool,        PlayOnStart,   "PlayOnStart",   true,          None)
    HH_PROP_NAMED(float,       CrossfadeTime, "CrossfadeTime", 0.2f,          None)
    HH_PROP_NAMED(std::string, Controller,    "Controller",    std::string{}, AssetRef)

    bool        Started      = false;        // runtime: Play has initialized this animator
    bool        Playing      = false;        // runtime: the current clip advances
    std::string CurrentClip;                 // runtime: the clip being played (Clip changing starts a new one)
    HedgehogAnimation::BlendStack Blend;     // runtime: the current clip, and those still fading out
    std::optional<uint64_t> BlendMesh;       // runtime: the mesh whose clips Blend's indices name
    std::optional<float> RequestedFade;      // runtime: the next switch's fade time (AnimationSystem::Play)
    std::optional<bool>  RequestedLoop;      // runtime: the next switch's loop (AnimationSystem::Play)
    std::optional<bool>  CurrentLoop;        // runtime: the current clip's own loop, else Loop
    bool        Finished     = false;        // runtime: a non-looping current clip reached its end
    std::string WarnedClip;                  // runtime: the unknown clip name already warned about
    std::optional<float> PreviewTime;        // runtime: in Edit mode, the time to show (else the bind pose)
    std::vector<HM::Matrix4x4> Palette;      // runtime: one skinning matrix per joint
    AnimatorControllerRuntime ControllerState; // runtime: Controller's machine
HH_END_COMPONENT(AnimatorComponent)
}
