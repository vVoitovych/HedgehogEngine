#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace HedgehogAnimation
{
    namespace
    {
        // Whether every point lies on one line through the first two distinct ones.
        bool AllOnOneLine(const std::vector<BlendChild>& children)
        {
            const std::array<float, 2>& a = children[0].Position;
            for (size_t i = 1; i < children.size(); ++i)
            {
                const std::array<float, 2>& b = children[i].Position;
                const float                 dx = b[0] - a[0];
                const float                 dy = b[1] - a[1];
                if (dx == 0.0f && dy == 0.0f)
                    continue;
                for (const BlendChild& child : children)
                {
                    const float cross = dx * (child.Position[1] - a[1]) - dy * (child.Position[0] - a[0]);
                    if (std::abs(cross) > 1e-5f * (std::abs(dx) + std::abs(dy)))
                        return false;
                }
                return true;
            }
            return true;
        }

        void CheckBlendParameter(const AnimatorController& controller, const AnimatorState& state,
                                 const std::string& name, const char* axis, std::vector<std::string>& problems)
        {
            const int parameter = name.empty() ? -1 : FindParameter(controller, name);
            if (parameter < 0 || controller.Parameters[parameter].Type != AnimatorParameterType::Float)
                problems.push_back("state '" + state.Name + "' blends by " + axis + " parameter '" + name +
                                   "', which is not a Float parameter");
        }

        void CheckBlendTree(const AnimatorController& controller, const AnimatorState& state,
                            const std::vector<std::string>* clipNames, std::vector<std::string>& problems)
        {
            const std::string where = "state '" + state.Name + "'";
            if (state.Children.empty())
                problems.push_back(where + " is a blend tree with no children");
            if (state.Children.size() > MAX_BLEND_TREE_CHILDREN)
                problems.push_back(where + " has " + std::to_string(state.Children.size()) + " children, more than " +
                                   std::to_string(MAX_BLEND_TREE_CHILDREN));
            const bool twoD = state.Motion == StateMotion::BlendTree2D;
            CheckBlendParameter(controller, state, state.BlendParameterX, twoD ? "x" : "its", problems);
            if (twoD)
                CheckBlendParameter(controller, state, state.BlendParameterY, "y", problems);

            for (size_t c = 0; c < state.Children.size(); ++c)
            {
                const BlendChild& child = state.Children[c];
                const std::string name  = where + " child " + std::to_string(c);
                if (!std::isfinite(child.Speed))
                    problems.push_back(name + " has a speed that is not a number");
                if (clipNames && std::find(clipNames->begin(), clipNames->end(), child.Clip) == clipNames->end())
                    problems.push_back(name + " plays clip '" + child.Clip + "', which the mesh does not have");
                if (!twoD && c > 0 && !(child.Threshold > state.Children[c - 1].Threshold))
                    problems.push_back(name + "'s threshold is not above child " + std::to_string(c - 1) + "'s");
                for (size_t other = 0; twoD && other < c; ++other)
                    if (state.Children[other].Position == child.Position)
                        problems.push_back(name + " is at the same position as child " + std::to_string(other));
            }
            if (twoD && state.Children.size() >= 3 && AllOnOneLine(state.Children))
                problems.push_back(where + "'s children all lie on one line, so its 2D blend cannot tell them apart");
        }

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

    const char* GetStateMotionName(StateMotion motion)
    {
        switch (motion)
        {
        case StateMotion::Clip:        return "Clip";
        case StateMotion::BlendTree1D: return "BlendTree1D";
        case StateMotion::BlendTree2D: return "BlendTree2D";
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
            if (state.Motion != StateMotion::Clip)
                CheckBlendTree(controller, state, clipNames, problems);
            else if (clipNames && std::find(clipNames->begin(), clipNames->end(), state.Clip) == clipNames->end())
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
