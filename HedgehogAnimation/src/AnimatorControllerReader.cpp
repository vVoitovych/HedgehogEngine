#include "HedgehogAnimation/api/AnimatorControllerFile.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace HedgehogAnimation
{
    namespace
    {
        // A parse failure, carrying the message up to ParseAnimatorController.
        struct ParseError
        {
            std::string Message;
        };

        constexpr std::array PARAMETER_TYPES = { AnimatorParameterType::Float, AnimatorParameterType::Int,
                                                 AnimatorParameterType::Bool, AnimatorParameterType::Trigger };
        constexpr std::array CONDITION_OPS   = { ConditionOp::Greater, ConditionOp::Less, ConditionOp::Equals,
                                                 ConditionOp::NotEquals, ConditionOp::True, ConditionOp::False,
                                                 ConditionOp::Triggered };

        std::string Scalar(const YAML::Node& node, const std::string& where)
        {
            if (!node.IsScalar())
                throw ParseError{ where + ": expected a single value" };
            return node.Scalar();
        }

        float Number(const YAML::Node& node, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            try
            {
                const float value = node.as<float>();
                if (!std::isfinite(value))
                    throw ParseError{ where + ": '" + text + "' is not a finite number" };
                return value;
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not a number" };
            }
        }

        bool Bool(const YAML::Node& node, const std::string& where)
        {
            const std::string text = Scalar(node, where);
            try
            {
                return node.as<bool>();
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not true or false" };
            }
        }

        // Every key of node is one of keys; the required ones are present.
        void CheckKeys(const YAML::Node& node, std::initializer_list<const char*> keys,
                       std::initializer_list<const char*> required, const std::string& where)
        {
            if (!node.IsMap())
                throw ParseError{ where + ": expected a map" };
            for (const auto& entry : node)
            {
                const std::string key = entry.first.Scalar();
                if (std::none_of(keys.begin(), keys.end(), [&](const char* known) { return key == known; }))
                    throw ParseError{ where + ": unknown key '" + key + "'" };
            }
            for (const char* key : required)
                if (!node[key])
                    throw ParseError{ where + ": " + key + " is missing" };
        }

        // The list under key, or none when it is left out or empty.
        YAML::Node List(const YAML::Node& root, const char* key)
        {
            const YAML::Node node = root[key];
            if (!node || node.IsNull())
                return YAML::Node(YAML::NodeType::Sequence);
            if (!node.IsSequence())
                throw ParseError{ std::string(key) + ": expected a list" };
            return node;
        }

        uint32_t StateIndex(const AnimatorController& controller, const YAML::Node& node, const std::string& where)
        {
            const std::string name  = Scalar(node, where);
            const int         index = FindState(controller, name);
            if (index < 0)
                throw ParseError{ where + ": there is no state '" + name + "'" };
            return static_cast<uint32_t>(index);
        }

        AnimatorParameter ReadParameter(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "Name", "Type", "Default" }, { "Name", "Type" }, where);
            AnimatorParameter parameter;
            parameter.Name           = Scalar(node["Name"], where);
            const std::string type   = Scalar(node["Type"], where);
            const auto        found  = std::find_if(PARAMETER_TYPES.begin(), PARAMETER_TYPES.end(),
                                                    [&](AnimatorParameterType t) { return type == GetParameterTypeName(t); });
            if (found == PARAMETER_TYPES.end())
                throw ParseError{ where + ": unknown type '" + type + "' (Float, Int, Bool or Trigger)" };
            parameter.Type = *found;

            if (const YAML::Node value = node["Default"])
            {
                switch (parameter.Type)
                {
                case AnimatorParameterType::Float:
                    parameter.Default = Number(value, where);
                    break;
                case AnimatorParameterType::Int:
                    parameter.Default = Number(value, where);
                    if (parameter.Default != std::floor(parameter.Default))
                        throw ParseError{ where + ": an Int parameter's default must be a whole number" };
                    break;
                case AnimatorParameterType::Bool:
                    parameter.Default = Bool(value, where) ? 1.0f : 0.0f;
                    break;
                case AnimatorParameterType::Trigger:
                    throw ParseError{ where + ": a Trigger parameter has no default" };
                }
            }
            return parameter;
        }

        AnimatorState ReadState(const YAML::Node& node, const std::string& where)
        {
            CheckKeys(node, { "Name", "Clip", "Speed", "Loop", "SpeedParameter" }, { "Name", "Clip" }, where);
            AnimatorState state;
            state.Name = Scalar(node["Name"], where);
            state.Clip = Scalar(node["Clip"], where);
            if (const YAML::Node speed = node["Speed"])
                state.Speed = Number(speed, where);
            if (const YAML::Node loop = node["Loop"])
                state.Loop = Bool(loop, where);
            if (const YAML::Node parameter = node["SpeedParameter"])
                state.SpeedParameter = Scalar(parameter, where);
            return state;
        }

        TransitionCondition ReadCondition(const AnimatorController& controller, const YAML::Node& node,
                                          const std::string& where)
        {
            CheckKeys(node, { "Parameter", "Op", "Value" }, { "Parameter", "Op" }, where);
            TransitionCondition condition;
            const std::string   name  = Scalar(node["Parameter"], where);
            const int           index = FindParameter(controller, name);
            if (index < 0)
                throw ParseError{ where + ": there is no parameter '" + name + "'" };
            condition.Parameter = static_cast<uint32_t>(index);

            const std::string op    = Scalar(node["Op"], where);
            const auto        found = std::find_if(CONDITION_OPS.begin(), CONDITION_OPS.end(),
                                                   [&](ConditionOp o) { return op == GetConditionOpName(o); });
            if (found == CONDITION_OPS.end())
                throw ParseError{ where + ": unknown operator '" + op + "'" };
            condition.Op = *found;

            const bool takesValue = condition.Op == ConditionOp::Greater || condition.Op == ConditionOp::Less ||
                                    condition.Op == ConditionOp::Equals || condition.Op == ConditionOp::NotEquals;
            if (takesValue && !node["Value"])
                throw ParseError{ where + ": " + op + " needs a Value" };
            if (!takesValue && node["Value"])
                throw ParseError{ where + ": " + op + " takes no Value" };
            if (takesValue)
                condition.Value = Number(node["Value"], where);
            return condition;
        }

        AnimatorTransition ReadTransition(const AnimatorController& controller, const YAML::Node& node,
                                          const std::string& where)
        {
            CheckKeys(node, { "From", "To", "ExitTime", "Duration", "CanTransitionToSelf", "Conditions" },
                      { "From", "To" }, where);
            AnimatorTransition transition;
            transition.From = Scalar(node["From"], where) == "Any" ? ANY_STATE
                                                                    : StateIndex(controller, node["From"], where + ", From");
            transition.To   = StateIndex(controller, node["To"], where + ", To");
            if (const YAML::Node exit = node["ExitTime"])
            {
                transition.HasExitTime = true;
                transition.ExitTime    = Number(exit, where);
            }
            if (const YAML::Node duration = node["Duration"])
                transition.Duration = Number(duration, where);
            if (const YAML::Node self = node["CanTransitionToSelf"])
                transition.CanTransitionToSelf = Bool(self, where);

            const YAML::Node conditions = List(node, "Conditions");
            for (size_t c = 0; c < conditions.size(); ++c)
                transition.Conditions.push_back(
                    ReadCondition(controller, conditions[c], where + ", condition " + std::to_string(c)));
            return transition;
        }
    }

    AnimatorControllerParseResult ParseAnimatorController(std::string_view text)
    {
        AnimatorControllerParseResult result;
        try
        {
            const YAML::Node root = YAML::Load(std::string(text));
            if (!root.IsMap())
                throw ParseError{ "the file is not a map of Version, Parameters, States, Default and Transitions" };
            CheckKeys(root, { "Version", "Parameters", "States", "Default", "Transitions" }, { "Version" }, "the file");
            const std::string version = Scalar(root["Version"], "Version");
            if (version != std::to_string(ANIMATOR_CONTROLLER_VERSION))
                throw ParseError{ "Version " + version + " is not supported (this build reads version " +
                                  std::to_string(ANIMATOR_CONTROLLER_VERSION) + ")" };

            AnimatorController controller;
            const YAML::Node   parameters = List(root, "Parameters");
            for (size_t i = 0; i < parameters.size(); ++i)
                controller.Parameters.push_back(ReadParameter(parameters[i], "parameter " + std::to_string(i)));
            const YAML::Node states = List(root, "States");
            for (size_t i = 0; i < states.size(); ++i)
                controller.States.push_back(ReadState(states[i], "state " + std::to_string(i)));
            if (!root["Default"])
                throw ParseError{ "Default is missing" };
            controller.DefaultState = StateIndex(controller, root["Default"], "Default");
            const YAML::Node transitions = List(root, "Transitions");
            for (size_t i = 0; i < transitions.size(); ++i)
                controller.Transitions.push_back(
                    ReadTransition(controller, transitions[i], "transition " + std::to_string(i)));
            result.Controller = std::move(controller);
        }
        catch (const ParseError& error)
        {
            result.Error = error.Message;
        }
        catch (const YAML::Exception& error)
        {
            result.Error = error.what();
        }
        return result;
    }
}
