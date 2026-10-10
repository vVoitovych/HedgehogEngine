#pragma once

#include <cstdint>
#include <string>
#include <vector>

// An animator controller (a .animctrl file, AnimatorControllerFile.hpp), as plain data: the
// parameters a script or system sets, the states (each playing one clip of the mesh) and the
// transitions between them, chosen by conditions on the parameters. StateMachine.hpp runs one.
namespace HedgehogAnimation
{
    // A transition's From when it may leave any state.
    inline constexpr uint32_t ANY_STATE = 0xFFFFFFFFu;

    enum class AnimatorParameterType
    {
        Float,
        Int,
        Bool,
        Trigger, // a bool that a transition using it resets
    };

    struct AnimatorParameter
    {
        std::string           Name;
        AnimatorParameterType Type    = AnimatorParameterType::Float;
        float                 Default = 0.0f; // an Int's whole number, a Bool's 0 or 1; a Trigger starts unset
    };

    struct AnimatorState
    {
        std::string Name;
        std::string Clip;  // a clip of the mesh, by name
        float       Speed = 1.0f;
        bool        Loop  = true;
        // A Float parameter the speed is multiplied by; none when empty.
        std::string SpeedParameter;
    };

    enum class ConditionOp
    {
        Greater,   // Float, Int: the value is above Value
        Less,      // Float, Int: below Value
        Equals,    // Int: equal to Value
        NotEquals, // Int
        True,      // Bool
        False,     // Bool
        Triggered, // Trigger: set
    };

    struct TransitionCondition
    {
        uint32_t    Parameter = 0; // an index into Parameters
        ConditionOp Op        = ConditionOp::Greater;
        float       Value     = 0.0f; // unused by True, False and Triggered
    };

    struct AnimatorTransition
    {
        uint32_t                         From = ANY_STATE; // a state index, or ANY_STATE
        uint32_t                         To   = 0;
        std::vector<TransitionCondition> Conditions;       // all must hold
        bool                             HasExitTime = false;
        float                            ExitTime    = 1.0f; // in clip lengths since the state began
        float                            Duration    = 0.25f; // the crossfade, in seconds
        // From ANY_STATE only: whether it may leave the state it leads to and start it again.
        bool CanTransitionToSelf = false;
    };

    struct AnimatorController
    {
        std::vector<AnimatorParameter>  Parameters;
        std::vector<AnimatorState>      States;
        std::vector<AnimatorTransition> Transitions; // checked in order
        uint32_t                        DefaultState = 0;
    };

    [[nodiscard]] const char* GetParameterTypeName(AnimatorParameterType type);
    [[nodiscard]] const char* GetConditionOpName(ConditionOp op);
    // Whether a condition with op can test a parameter of type.
    [[nodiscard]] bool IsConditionOpValid(AnimatorParameterType type, ConditionOp op);

    // The index of the state or parameter of that name; -1 when there is none.
    [[nodiscard]] int FindState(const AnimatorController& controller, const std::string& name);
    [[nodiscard]] int FindParameter(const AnimatorController& controller, const std::string& name);

    // What is wrong with controller, one message each naming the state, parameter or transition
    // (by number, from 0): no states, a default state out of range, an empty or repeated name, a
    // state or parameter index out of range, a condition that does not fit its parameter's type,
    // a speed parameter that is not a Float parameter, a transition with neither a condition nor
    // an exit time (it would fire at once), a negative duration or exit time, and, given the
    // mesh's clip names, a state whose clip the mesh lacks. Empty when it can run.
    [[nodiscard]] std::vector<std::string> ValidateAnimatorController(const AnimatorController& controller,
                                                                      const std::vector<std::string>* clipNames = nullptr);
}
