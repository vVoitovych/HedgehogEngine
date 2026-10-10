#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace HedgehogAnimation
{
    namespace
    {
        std::string Transition(size_t index)
        {
            return "transition " + std::to_string(index);
        }

        // Names must be present and unique: what the file and the scripts find them by.
        template <typename T>
        void CheckNames(const std::vector<T>& items, const char* kind, std::vector<std::string>& problems)
        {
            std::unordered_set<std::string> seen;
            for (size_t i = 0; i < items.size(); ++i)
            {
                if (items[i].Name.empty())
                    problems.push_back(std::string(kind) + " " + std::to_string(i) + " has no name");
                else if (!seen.insert(items[i].Name).second)
                    problems.push_back(std::string(kind) + " '" + items[i].Name + "' is named twice");
            }
        }
    }

    const char* GetParameterTypeName(AnimatorParameterType type)
    {
        switch (type)
        {
        case AnimatorParameterType::Float:   return "Float";
        case AnimatorParameterType::Int:     return "Int";
        case AnimatorParameterType::Bool:    return "Bool";
        case AnimatorParameterType::Trigger: return "Trigger";
        }
        return "";
    }

    const char* GetConditionOpName(ConditionOp op)
    {
        switch (op)
        {
        case ConditionOp::Greater:   return "Greater";
        case ConditionOp::Less:      return "Less";
        case ConditionOp::Equals:    return "Equals";
        case ConditionOp::NotEquals: return "NotEquals";
        case ConditionOp::True:      return "True";
        case ConditionOp::False:     return "False";
        case ConditionOp::Triggered: return "Triggered";
        }
        return "";
    }

    bool IsConditionOpValid(AnimatorParameterType type, ConditionOp op)
    {
        switch (type)
        {
        case AnimatorParameterType::Float:   return op == ConditionOp::Greater || op == ConditionOp::Less;
        case AnimatorParameterType::Int:     return op == ConditionOp::Greater || op == ConditionOp::Less ||
                                                    op == ConditionOp::Equals || op == ConditionOp::NotEquals;
        case AnimatorParameterType::Bool:    return op == ConditionOp::True || op == ConditionOp::False;
        case AnimatorParameterType::Trigger: return op == ConditionOp::Triggered;
        }
        return false;
    }

    int FindState(const AnimatorController& controller, const std::string& name)
    {
        for (size_t i = 0; i < controller.States.size(); ++i)
            if (controller.States[i].Name == name)
                return static_cast<int>(i);
        return -1;
    }

    int FindParameter(const AnimatorController& controller, const std::string& name)
    {
        for (size_t i = 0; i < controller.Parameters.size(); ++i)
            if (controller.Parameters[i].Name == name)
                return static_cast<int>(i);
        return -1;
    }

    std::vector<std::string> ValidateAnimatorController(const AnimatorController& controller,
                                                        const std::vector<std::string>* clipNames)
    {
        std::vector<std::string> problems;
        const size_t             states = controller.States.size();
        if (states == 0)
            problems.push_back("the controller has no states");
        else if (controller.DefaultState >= states)
            problems.push_back("the default state is not one of its states");
        CheckNames(controller.States, "state", problems);
        CheckNames(controller.Parameters, "parameter", problems);

        for (const AnimatorState& state : controller.States)
        {
            if (!std::isfinite(state.Speed))
                problems.push_back("state '" + state.Name + "' has a speed that is not a number");
            if (clipNames && std::find(clipNames->begin(), clipNames->end(), state.Clip) == clipNames->end())
                problems.push_back("state '" + state.Name + "' plays clip '" + state.Clip + "', which the mesh does not have");
            if (!state.SpeedParameter.empty())
            {
                const int parameter = FindParameter(controller, state.SpeedParameter);
                if (parameter < 0 || controller.Parameters[parameter].Type != AnimatorParameterType::Float)
                    problems.push_back("state '" + state.Name + "' takes its speed from '" + state.SpeedParameter +
                                       "', which is not a Float parameter");
            }
        }

        for (size_t t = 0; t < controller.Transitions.size(); ++t)
        {
            const AnimatorTransition& transition = controller.Transitions[t];
            if (transition.From != ANY_STATE && transition.From >= states)
                problems.push_back(Transition(t) + " leaves a state that does not exist");
            if (transition.To >= states)
                problems.push_back(Transition(t) + " leads to a state that does not exist");
            if (transition.Conditions.empty() && !transition.HasExitTime)
                problems.push_back(Transition(t) + " has neither a condition nor an exit time, so it would fire at once");
            if (!(transition.Duration >= 0.0f) || !std::isfinite(transition.Duration))
                problems.push_back(Transition(t) + " has a duration that is not 0 or more seconds");
            if (transition.HasExitTime && (!(transition.ExitTime >= 0.0f) || !std::isfinite(transition.ExitTime)))
                problems.push_back(Transition(t) + " has an exit time that is not 0 or more");
            for (size_t c = 0; c < transition.Conditions.size(); ++c)
            {
                const TransitionCondition& condition = transition.Conditions[c];
                const std::string          where     = Transition(t) + ", condition " + std::to_string(c);
                if (condition.Parameter >= controller.Parameters.size())
                {
                    problems.push_back(where + " tests a parameter that does not exist");
                    continue;
                }
                const AnimatorParameter& parameter = controller.Parameters[condition.Parameter];
                if (!IsConditionOpValid(parameter.Type, condition.Op))
                    problems.push_back(where + ": " + GetConditionOpName(condition.Op) + " cannot test " +
                                       GetParameterTypeName(parameter.Type) + " parameter '" + parameter.Name + "'");
            }
        }
        return problems;
    }
}
