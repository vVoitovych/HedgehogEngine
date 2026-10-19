#include "ControllerEditing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace Editor
{
    namespace
    {
        using HedgehogAnimation::AnimatorController;
        using HedgehogAnimation::AnimatorParameterType;
        using HedgehogAnimation::ANY_STATE;

        template <typename T>
        bool HasName(const std::vector<T>& items, std::string_view name, size_t except = SIZE_MAX)
        {
            for (size_t i = 0; i < items.size(); ++i)
                if (i != except && items[i].Name == name)
                    return true;
            return false;
        }

        template <typename T>
        std::string MakeUniqueName(const std::vector<T>& items, std::string_view name, std::string_view fallback)
        {
            const std::string base(name.empty() ? fallback : name);
            if (!HasName(items, base))
                return base;
            for (int number = 1;; ++number)
            {
                std::string candidate = base + " " + std::to_string(number);
                if (!HasName(items, candidate))
                    return candidate;
            }
        }

        // A default that suits the type.
        float DefaultFor(AnimatorParameterType type, float value)
        {
            switch (type)
            {
            case AnimatorParameterType::Float:   return value;
            case AnimatorParameterType::Int:     return std::round(value);
            case AnimatorParameterType::Bool:    return value != 0.0f ? 1.0f : 0.0f;
            case AnimatorParameterType::Trigger: return 0.0f;
            }
            return 0.0f;
        }

        // A transition with nothing left to wait for waits for a whole play of its clip.
        void KeepWaiting(HedgehogAnimation::AnimatorTransition& transition)
        {
            if (transition.Conditions.empty() && !transition.HasExitTime)
            {
                transition.HasExitTime = true;
                transition.ExitTime    = 1.0f;
            }
        }
    }

    std::string MakeUniqueStateName(const AnimatorController& controller, std::string_view name)
    {
        return MakeUniqueName(controller.States, name, "State");
    }

    std::string MakeUniqueParameterName(const AnimatorController& controller, std::string_view name)
    {
        return MakeUniqueName(controller.Parameters, name, "Parameter");
    }

    uint32_t AddState(AnimatorController& controller, std::string_view name, std::string_view clip)
    {
        HedgehogAnimation::AnimatorState state;
        state.Name = MakeUniqueStateName(controller, name);
        state.Clip = std::string(clip);
        if (controller.States.empty())
            controller.DefaultState = 0;
        controller.States.push_back(std::move(state));
        return static_cast<uint32_t>(controller.States.size() - 1);
    }

    bool RenameState(AnimatorController& controller, uint32_t state, std::string_view name)
    {
        if (state >= controller.States.size() || name.empty() || HasName(controller.States, name, state))
            return false;
        controller.States[state].Name = std::string(name);
        return true;
    }

    bool DeleteState(AnimatorController& controller, uint32_t state)
    {
        if (state >= controller.States.size())
            return false;
        controller.States.erase(controller.States.begin() + state);
        std::erase_if(controller.Transitions, [state](const HedgehogAnimation::AnimatorTransition& transition)
                      { return transition.From == state || transition.To == state; });
        const auto shift = [state](uint32_t& index)
        {
            if (index != ANY_STATE && index > state)
                --index;
        };
        for (HedgehogAnimation::AnimatorTransition& transition : controller.Transitions)
        {
            shift(transition.From);
            shift(transition.To);
        }
        if (controller.DefaultState == state)
            controller.DefaultState = 0;
        else if (controller.DefaultState > state)
            --controller.DefaultState;
        return true;
    }

    bool SetDefaultState(AnimatorController& controller, uint32_t state)
    {
        if (state >= controller.States.size())
            return false;
        controller.DefaultState = state;
        return true;
    }

    uint32_t AddParameter(AnimatorController& controller, std::string_view name, AnimatorParameterType type)
    {
        HedgehogAnimation::AnimatorParameter parameter;
        parameter.Name = MakeUniqueParameterName(controller, name);
        parameter.Type = type;
        controller.Parameters.push_back(std::move(parameter));
        return static_cast<uint32_t>(controller.Parameters.size() - 1);
    }

    bool RenameParameter(AnimatorController& controller, uint32_t parameter, std::string_view name)
    {
        if (parameter >= controller.Parameters.size() || name.empty() || HasName(controller.Parameters, name, parameter))
            return false;
        std::string& current = controller.Parameters[parameter].Name;
        for (HedgehogAnimation::AnimatorState& state : controller.States)
            if (state.SpeedParameter == current)
                state.SpeedParameter = std::string(name);
        current = std::string(name);
        return true;
    }

    bool DeleteParameter(AnimatorController& controller, uint32_t parameter)
    {
        if (parameter >= controller.Parameters.size())
            return false;
        const std::string name = controller.Parameters[parameter].Name;
        controller.Parameters.erase(controller.Parameters.begin() + parameter);
        for (HedgehogAnimation::AnimatorState& state : controller.States)
            if (state.SpeedParameter == name)
                state.SpeedParameter.clear();
        for (HedgehogAnimation::AnimatorTransition& transition : controller.Transitions)
        {
            std::erase_if(transition.Conditions, [parameter](const HedgehogAnimation::TransitionCondition& condition)
                          { return condition.Parameter == parameter; });
            for (HedgehogAnimation::TransitionCondition& condition : transition.Conditions)
                if (condition.Parameter > parameter)
                    --condition.Parameter;
            KeepWaiting(transition);
        }
        return true;
    }

    bool SetParameterType(AnimatorController& controller, uint32_t parameter, AnimatorParameterType type)
    {
        if (parameter >= controller.Parameters.size())
            return false;
        HedgehogAnimation::AnimatorParameter& changed = controller.Parameters[parameter];
        changed.Type    = type;
        changed.Default = DefaultFor(type, changed.Default);
        for (HedgehogAnimation::AnimatorTransition& transition : controller.Transitions)
            for (HedgehogAnimation::TransitionCondition& condition : transition.Conditions)
                if (condition.Parameter == parameter && !HedgehogAnimation::IsConditionOpValid(type, condition.Op))
                {
                    condition.Op    = GetDefaultConditionOp(type);
                    condition.Value = 0.0f;
                }
        if (type != AnimatorParameterType::Float)
            for (HedgehogAnimation::AnimatorState& state : controller.States)
                if (state.SpeedParameter == changed.Name)
                    state.SpeedParameter.clear();
        return true;
    }

    HedgehogAnimation::ConditionOp GetDefaultConditionOp(AnimatorParameterType type)
    {
        switch (type)
        {
        case AnimatorParameterType::Float:
        case AnimatorParameterType::Int:     return HedgehogAnimation::ConditionOp::Greater;
        case AnimatorParameterType::Bool:    return HedgehogAnimation::ConditionOp::True;
        case AnimatorParameterType::Trigger: return HedgehogAnimation::ConditionOp::Triggered;
        }
        return HedgehogAnimation::ConditionOp::Greater;
    }

    std::optional<uint32_t> AddTransition(AnimatorController& controller, uint32_t from, uint32_t to)
    {
        if ((from != ANY_STATE && from >= controller.States.size()) || to >= controller.States.size())
            return std::nullopt;
        HedgehogAnimation::AnimatorTransition transition;
        transition.From        = from;
        transition.To          = to;
        transition.HasExitTime = true;
        transition.ExitTime    = 1.0f;
        controller.Transitions.push_back(std::move(transition));
        return static_cast<uint32_t>(controller.Transitions.size() - 1);
    }

    bool DeleteTransition(AnimatorController& controller, uint32_t transition)
    {
        if (transition >= controller.Transitions.size())
            return false;
        controller.Transitions.erase(controller.Transitions.begin() + transition);
        return true;
    }

    bool MoveTransition(AnimatorController& controller, uint32_t transition, uint32_t place)
    {
        auto& transitions = controller.Transitions;
        if (transition >= transitions.size() || place >= transitions.size())
            return false;
        if (transition < place)
            std::rotate(transitions.begin() + transition, transitions.begin() + transition + 1,
                        transitions.begin() + place + 1);
        else
            std::rotate(transitions.begin() + place, transitions.begin() + transition,
                        transitions.begin() + transition + 1);
        return true;
    }

    std::optional<uint32_t> AddCondition(AnimatorController& controller, uint32_t transition, uint32_t parameter)
    {
        if (transition >= controller.Transitions.size() || parameter >= controller.Parameters.size())
            return std::nullopt;
        HedgehogAnimation::TransitionCondition condition;
        condition.Parameter = parameter;
        condition.Op        = GetDefaultConditionOp(controller.Parameters[parameter].Type);
        auto& conditions    = controller.Transitions[transition].Conditions;
        conditions.push_back(condition);
        return static_cast<uint32_t>(conditions.size() - 1);
    }

    bool DeleteCondition(AnimatorController& controller, uint32_t transition, uint32_t condition)
    {
        if (transition >= controller.Transitions.size() ||
            condition >= controller.Transitions[transition].Conditions.size())
            return false;
        auto& conditions = controller.Transitions[transition].Conditions;
        conditions.erase(conditions.begin() + condition);
        KeepWaiting(controller.Transitions[transition]);
        return true;
    }

    bool SetConditionParameter(AnimatorController& controller, uint32_t transition, uint32_t condition,
                               uint32_t parameter)
    {
        if (transition >= controller.Transitions.size() || parameter >= controller.Parameters.size() ||
            condition >= controller.Transitions[transition].Conditions.size())
            return false;
        HedgehogAnimation::TransitionCondition& changed = controller.Transitions[transition].Conditions[condition];
        const AnimatorParameterType             type    = controller.Parameters[parameter].Type;
        changed.Parameter                               = parameter;
        if (!HedgehogAnimation::IsConditionOpValid(type, changed.Op))
        {
            changed.Op    = GetDefaultConditionOp(type);
            changed.Value = 0.0f;
        }
        return true;
    }
}
