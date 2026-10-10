#include "HedgehogAnimation/api/StateMachine.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogAnimation
{
    namespace
    {
        // The index of a parameter called name of type; -1 when there is none.
        int FindParameterOfType(const AnimatorController& controller, const std::string& name, AnimatorParameterType type)
        {
            const int index = FindParameter(controller, name);
            return index >= 0 && controller.Parameters[index].Type == type ? index : -1;
        }

        float NormalizedTime(float time, float duration)
        {
            return duration > 0.0f ? time / duration : std::max(time, 0.0f) + 1.0f;
        }

        bool ConditionHolds(const TransitionCondition& condition, const StateMachineInstance& instance)
        {
            const float value = condition.Parameter < instance.Values.size() ? instance.Values[condition.Parameter] : 0.0f;
            switch (condition.Op)
            {
            case ConditionOp::Greater:   return value > condition.Value;
            case ConditionOp::Less:      return value < condition.Value;
            case ConditionOp::Equals:    return value == condition.Value;
            case ConditionOp::NotEquals: return value != condition.Value;
            case ConditionOp::True:
            case ConditionOp::Triggered: return value != 0.0f;
            case ConditionOp::False:     return value == 0.0f;
            }
            return false;
        }

        bool ExitTimeReached(const AnimatorTransition& transition, bool loop, float before, float now)
        {
            if (!transition.HasExitTime)
                return true;
            if (!loop || transition.ExitTime >= 1.0f)
                return now >= transition.ExitTime;
            // A point in every loop: past it in this loop, or a loop's end crossed this step.
            return now - std::floor(now) >= transition.ExitTime || std::floor(now) > std::floor(before);
        }

        bool MayFire(const AnimatorTransition& transition, const AnimatorController& controller,
                     const StateMachineInstance& instance, float before, float now)
        {
            if (transition.From == ANY_STATE && transition.To == instance.State && !transition.CanTransitionToSelf)
                return false;
            const bool loop = controller.States[instance.State].Loop;
            if (!ExitTimeReached(transition, loop, before, now))
                return false;
            for (const TransitionCondition& condition : transition.Conditions)
                if (!ConditionHolds(condition, instance))
                    return false;
            return true;
        }

        TransitionFired Fire(const AnimatorTransition& transition, StateMachineInstance& instance)
        {
            for (const TransitionCondition& condition : transition.Conditions)
                if (condition.Op == ConditionOp::Triggered && condition.Parameter < instance.Values.size())
                    instance.Values[condition.Parameter] = 0.0f;
            const TransitionFired fired{ instance.State, transition.To, transition.Duration };
            instance.State     = transition.To;
            instance.StateTime = 0.0f;
            return fired;
        }
    }

    void ResetStateMachine(const AnimatorController& controller, StateMachineInstance& instance)
    {
        instance.State     = controller.DefaultState;
        instance.StateTime = 0.0f;
        instance.Values.resize(controller.Parameters.size());
        for (size_t i = 0; i < controller.Parameters.size(); ++i)
            instance.Values[i] =
                controller.Parameters[i].Type == AnimatorParameterType::Trigger ? 0.0f : controller.Parameters[i].Default;
    }

    bool SetFloat(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, float value)
    {
        const int index = FindParameterOfType(controller, name, AnimatorParameterType::Float);
        if (index < 0 || static_cast<size_t>(index) >= instance.Values.size())
            return false;
        instance.Values[index] = value;
        return true;
    }

    bool SetInt(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, int value)
    {
        const int index = FindParameterOfType(controller, name, AnimatorParameterType::Int);
        if (index < 0 || static_cast<size_t>(index) >= instance.Values.size())
            return false;
        instance.Values[index] = static_cast<float>(value);
        return true;
    }

    bool SetBool(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, bool value)
    {
        const int index = FindParameterOfType(controller, name, AnimatorParameterType::Bool);
        if (index < 0 || static_cast<size_t>(index) >= instance.Values.size())
            return false;
        instance.Values[index] = value ? 1.0f : 0.0f;
        return true;
    }

    bool SetTrigger(const AnimatorController& controller, StateMachineInstance& instance, const std::string& name, bool set)
    {
        const int index = FindParameterOfType(controller, name, AnimatorParameterType::Trigger);
        if (index < 0 || static_cast<size_t>(index) >= instance.Values.size())
            return false;
        instance.Values[index] = set ? 1.0f : 0.0f;
        return true;
    }

    std::optional<float> GetParameter(const AnimatorController& controller, const StateMachineInstance& instance,
                                      const std::string& name)
    {
        const int index = FindParameter(controller, name);
        if (index < 0 || static_cast<size_t>(index) >= instance.Values.size())
            return std::nullopt;
        return instance.Values[index];
    }

    float GetNormalizedTime(const StateMachineInstance& instance, std::span<const float> clipDurations)
    {
        const float duration = instance.State < clipDurations.size() ? clipDurations[instance.State] : 0.0f;
        return NormalizedTime(instance.StateTime, duration);
    }

    std::optional<TransitionFired> StepStateMachine(const AnimatorController& controller, StateMachineInstance& instance,
                                                    std::span<const float> clipDurations, float dt)
    {
        if (instance.State >= controller.States.size())
            return std::nullopt;

        const AnimatorState& state = controller.States[instance.State];
        float                speed = state.Speed;
        if (!state.SpeedParameter.empty())
        {
            const int parameter = FindParameterOfType(controller, state.SpeedParameter, AnimatorParameterType::Float);
            if (parameter >= 0 && static_cast<size_t>(parameter) < instance.Values.size())
                speed *= instance.Values[parameter];
        }

        const float before = GetNormalizedTime(instance, clipDurations);
        instance.StateTime += dt * speed;
        const float now = GetNormalizedTime(instance, clipDurations);

        for (const AnimatorTransition& transition : controller.Transitions)
            if (transition.From == ANY_STATE && MayFire(transition, controller, instance, before, now))
                return Fire(transition, instance);
        for (const AnimatorTransition& transition : controller.Transitions)
            if (transition.From == instance.State && MayFire(transition, controller, instance, before, now))
                return Fire(transition, instance);
        return std::nullopt;
    }
}
