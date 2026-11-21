#include "ControllerEditing.hpp"

#include <algorithm>
#include <array>
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

        // Every state's speed and blend parameters naming from now name to (empty to clear them).
        void RenameStateParameters(AnimatorController& controller, const std::string& from, const std::string& to,
                                   bool speeds)
        {
            for (HedgehogAnimation::AnimatorState& state : controller.States)
            {
                if (speeds && state.SpeedParameter == from)
                    state.SpeedParameter = to;
                if (state.BlendParameterX == from)
                    state.BlendParameterX = to;
                if (state.BlendParameterY == from)
                    state.BlendParameterY = to;
            }
        }

        bool IsTree(const AnimatorController& controller, uint32_t state)
        {
            return state < controller.States.size() &&
                   controller.States[state].Motion != HedgehogAnimation::StateMotion::Clip;
        }

        bool AllOnOneLine(const std::vector<HedgehogAnimation::BlendChild>& children)
        {
            for (size_t i = 1; i < children.size(); ++i)
            {
                const float dx = children[i].Position[0] - children[0].Position[0];
                const float dy = children[i].Position[1] - children[0].Position[1];
                if (dx == 0.0f && dy == 0.0f)
                    continue;
                for (const HedgehogAnimation::BlendChild& child : children)
                {
                    const float cross = dx * (child.Position[1] - children[0].Position[1]) -
                                        dy * (child.Position[0] - children[0].Position[0]);
                    if (std::abs(cross) > 1e-5f * (std::abs(dx) + std::abs(dy)))
                        return false;
                }
                return true;
            }
            return true;
        }

        // The first Float parameter, or none.
        std::string FirstFloat(const AnimatorController& controller)
        {
            for (const HedgehogAnimation::AnimatorParameter& parameter : controller.Parameters)
                if (parameter.Type == AnimatorParameterType::Float)
                    return parameter.Name;
            return {};
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
        RenameStateParameters(controller, current, std::string(name), true);
        current = std::string(name);
        return true;
    }

    bool DeleteParameter(AnimatorController& controller, uint32_t parameter)
    {
        if (parameter >= controller.Parameters.size())
            return false;
        const std::string name = controller.Parameters[parameter].Name;
        controller.Parameters.erase(controller.Parameters.begin() + parameter);
        RenameStateParameters(controller, name, {}, true);
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
            RenameStateParameters(controller, changed.Name, {}, true);
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

    bool SetStateMotion(AnimatorController& controller, uint32_t state, HedgehogAnimation::StateMotion motion)
    {
        if (state >= controller.States.size())
            return false;
        HedgehogAnimation::AnimatorState& changed = controller.States[state];
        const HedgehogAnimation::StateMotion was = changed.Motion;
        if (was == motion)
            return true;
        changed.Motion = motion;
        if (motion == HedgehogAnimation::StateMotion::Clip)
        {
            if (!changed.Children.empty())
                changed.Clip = changed.Children.front().Clip;
            changed.Children.clear();
            changed.BlendParameterX.clear();
            changed.BlendParameterY.clear();
            return true;
        }
        const bool twoD = motion == HedgehogAnimation::StateMotion::BlendTree2D;
        if (was == HedgehogAnimation::StateMotion::Clip)
        {
            changed.Children        = { { changed.Clip }, { changed.Clip } };
            changed.BlendParameterX = FirstFloat(controller);
            changed.Clip.clear();
        }
        for (size_t c = 0; c < changed.Children.size(); ++c)
        {
            HedgehogAnimation::BlendChild& child = changed.Children[c];
            if (twoD)
                child.Position = { was == HedgehogAnimation::StateMotion::Clip ? static_cast<float>(c) : child.Threshold, 0.0f };
            else
                child.Threshold = static_cast<float>(c);
        }
        if (twoD && changed.Children.size() >= 3)
        {
            // Laid along x they would be on one line: the third and later go up and down from it.
            for (size_t c = 2; c < changed.Children.size(); ++c)
                changed.Children[c].Position = { static_cast<float>(c) - 2.0f, c % 2 == 0 ? 1.0f : -1.0f };
        }
        changed.BlendParameterY = twoD ? (changed.BlendParameterY.empty() ? changed.BlendParameterX : changed.BlendParameterY)
                                       : std::string{};
        return true;
    }

    std::optional<uint32_t> AddBlendChild(AnimatorController& controller, uint32_t state, std::string_view clip)
    {
        if (!IsTree(controller, state))
            return std::nullopt;
        HedgehogAnimation::AnimatorState& tree = controller.States[state];
        if (tree.Children.size() >= HedgehogAnimation::MAX_BLEND_TREE_CHILDREN)
            return std::nullopt;
        HedgehogAnimation::BlendChild child{ std::string(clip) };
        if (tree.Motion == HedgehogAnimation::StateMotion::BlendTree1D)
            child.Threshold = tree.Children.empty() ? 0.0f : tree.Children.back().Threshold + 1.0f;
        else
        {
            // The first place around the origin no child has that leaves them not all on one line.
            constexpr std::array<std::array<float, 2>, 17> CANDIDATES = {
                { { 0, 0 }, { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 }, { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 },
                  { 2, 0 }, { 0, 2 }, { -2, 0 }, { 0, -2 }, { 2, 2 }, { -2, 2 }, { -2, -2 }, { 2, -2 } }
            };
            for (const std::array<float, 2>& candidate : CANDIDATES)
            {
                const bool taken = std::any_of(tree.Children.begin(), tree.Children.end(),
                                               [&](const HedgehogAnimation::BlendChild& other) { return other.Position == candidate; });
                if (taken)
                    continue;
                std::vector<HedgehogAnimation::BlendChild> trial = tree.Children;
                trial.push_back({ child.Clip, 0.0f, candidate });
                if (trial.size() < 3 || !AllOnOneLine(trial))
                {
                    child.Position = candidate;
                    break;
                }
            }
        }
        tree.Children.push_back(child);
        return static_cast<uint32_t>(tree.Children.size() - 1);
    }

    bool DeleteBlendChild(AnimatorController& controller, uint32_t state, uint32_t child)
    {
        if (!IsTree(controller, state))
            return false;
        std::vector<HedgehogAnimation::BlendChild>& children = controller.States[state].Children;
        if (child >= children.size() || children.size() == 1)
            return false;
        std::vector<HedgehogAnimation::BlendChild> left = children;
        left.erase(left.begin() + child);
        if (controller.States[state].Motion == HedgehogAnimation::StateMotion::BlendTree2D && left.size() >= 3 &&
            AllOnOneLine(left))
            return false;
        children = std::move(left);
        return true;
    }

    bool MoveBlendChild(AnimatorController& controller, uint32_t state, uint32_t child, uint32_t place)
    {
        if (!IsTree(controller, state))
            return false;
        std::vector<HedgehogAnimation::BlendChild>& children = controller.States[state].Children;
        if (child >= children.size() || place >= children.size())
            return false;
        std::vector<float> thresholds;
        for (const HedgehogAnimation::BlendChild& each : children)
            thresholds.push_back(each.Threshold);
        if (place < child)
            std::rotate(children.begin() + place, children.begin() + child, children.begin() + child + 1);
        else
            std::rotate(children.begin() + child, children.begin() + child + 1, children.begin() + place + 1);
        if (controller.States[state].Motion == HedgehogAnimation::StateMotion::BlendTree1D)
        {
            std::sort(thresholds.begin(), thresholds.end());
            for (size_t c = 0; c < children.size(); ++c)
                children[c].Threshold = thresholds[c];
        }
        return true;
    }

    bool SetBlendParameter(AnimatorController& controller, uint32_t state, uint32_t axis, std::string_view parameter)
    {
        if (!IsTree(controller, state) ||
            (axis == 1 && controller.States[state].Motion != HedgehogAnimation::StateMotion::BlendTree2D) || axis > 1)
            return false;
        if (!parameter.empty())
        {
            const int index = HedgehogAnimation::FindParameter(controller, std::string(parameter));
            if (index < 0 || controller.Parameters[static_cast<size_t>(index)].Type != AnimatorParameterType::Float)
                return false;
        }
        (axis == 0 ? controller.States[state].BlendParameterX : controller.States[state].BlendParameterY) =
            std::string(parameter);
        return true;
    }
}
