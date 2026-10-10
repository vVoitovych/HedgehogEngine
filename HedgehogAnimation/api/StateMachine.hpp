#pragma once

#include "AnimatorController.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

// Runs an AnimatorController: plain data for one animator's machine, and free functions over it.
// The machine only chooses states; playing their clips (and crossfading over a transition's
// duration) is the caller's job.
namespace HedgehogAnimation
{
    struct StateMachineInstance
    {
        uint32_t           State     = 0;    // the current state's index
        float              StateTime = 0.0f; // seconds of its clip played since it began (its speed applied)
        std::vector<float> Values;           // one per parameter: a Float's or Int's value, 1 or 0 for a Bool or Trigger
    };

    struct TransitionFired
    {
        uint32_t From     = 0; // the state left
        uint32_t To       = 0; // the state entered, which starts from its clip's start
        float    Duration = 0.0f;
    };

    // The default state from time 0, every parameter at its default and every trigger unset.
    void ResetStateMachine(const AnimatorController& controller, StateMachineInstance& instance);

    // Sets the parameter called name. False, changing nothing, for a name that is not a parameter
    // of that type.
    bool SetFloat(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, float value);
    bool SetInt(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, int value);
    bool SetBool(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, bool value);
    // Sets or unsets a Trigger, which stays set until a transition testing it fires.
    bool SetTrigger(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name,
                    bool set = true);

    // The parameter's value as stored; nullopt for a name that is not a parameter.
    [[nodiscard]] std::optional<float> GetParameter(const AnimatorController& controller,
                                                    const StateMachineInstance& instance, const std::string& name);

    // The current state's time in lengths of its clip (clipDurations[state], seconds; 0 when its
    // clip is unknown, which counts as already played through).
    [[nodiscard]] float GetNormalizedTime(const StateMachineInstance& instance, std::span<const float> clipDurations);

    // One step of dt seconds: the current state's time advances by dt times its speed (and its
    // speed parameter), then the first transition that may fire does, Any State transitions
    // before the current state's own, each list in file order. A transition may fire when every
    // condition holds and, with an exit time, the state has played that many lengths of its clip
    // (on a looping state an exit time below 1 is a point in every loop, reached when the time is
    // past it in the current loop or crossed a loop's end this step). An Any State transition to
    // the current state fires only with CanTransitionToSelf. The triggers it tested are unset,
    // and the new state starts from time 0. At most one transition per step; nullopt when none
    // fired. clipDurations has one entry per state; a controller ValidateAnimatorController finds
    // fault with must not be run.
    std::optional<TransitionFired> StepStateMachine(const AnimatorController& controller, StateMachineInstance& instance,
                                                    std::span<const float> clipDurations, float dt);
}
