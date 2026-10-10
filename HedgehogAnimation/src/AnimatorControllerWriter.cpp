#include "HedgehogAnimation/api/AnimatorControllerFile.hpp"

#include <yaml-cpp/yaml.h>

namespace HedgehogAnimation
{
    namespace
    {
        std::string StateName(const AnimatorController& controller, uint32_t index)
        {
            return index < controller.States.size() ? controller.States[index].Name : std::string{};
        }

        void WriteParameter(YAML::Emitter& out, const AnimatorParameter& parameter)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << parameter.Name;
            out << YAML::Key << "Type" << YAML::Value << GetParameterTypeName(parameter.Type);
            if (parameter.Type == AnimatorParameterType::Bool && parameter.Default != 0.0f)
                out << YAML::Key << "Default" << YAML::Value << true;
            else if ((parameter.Type == AnimatorParameterType::Float || parameter.Type == AnimatorParameterType::Int) &&
                     parameter.Default != 0.0f)
                out << YAML::Key << "Default" << YAML::Value << parameter.Default;
            out << YAML::EndMap;
        }

        void WriteState(YAML::Emitter& out, const AnimatorState& state)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "Name" << YAML::Value << state.Name;
            out << YAML::Key << "Clip" << YAML::Value << state.Clip;
            if (state.Speed != 1.0f)
                out << YAML::Key << "Speed" << YAML::Value << state.Speed;
            if (!state.Loop)
                out << YAML::Key << "Loop" << YAML::Value << false;
            if (!state.SpeedParameter.empty())
                out << YAML::Key << "SpeedParameter" << YAML::Value << state.SpeedParameter;
            out << YAML::EndMap;
        }

        void WriteCondition(YAML::Emitter& out, const AnimatorController& controller, const TransitionCondition& condition)
        {
            const std::string parameter =
                condition.Parameter < controller.Parameters.size() ? controller.Parameters[condition.Parameter].Name : "";
            out << YAML::Flow << YAML::BeginMap;
            out << YAML::Key << "Parameter" << YAML::Value << parameter;
            out << YAML::Key << "Op" << YAML::Value << GetConditionOpName(condition.Op);
            if (condition.Op == ConditionOp::Greater || condition.Op == ConditionOp::Less ||
                condition.Op == ConditionOp::Equals || condition.Op == ConditionOp::NotEquals)
                out << YAML::Key << "Value" << YAML::Value << condition.Value;
            out << YAML::EndMap;
        }

        void WriteTransition(YAML::Emitter& out, const AnimatorController& controller, const AnimatorTransition& transition)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "From" << YAML::Value
                << (transition.From == ANY_STATE ? std::string("Any") : StateName(controller, transition.From));
            out << YAML::Key << "To" << YAML::Value << StateName(controller, transition.To);
            if (transition.HasExitTime)
                out << YAML::Key << "ExitTime" << YAML::Value << transition.ExitTime;
            if (transition.Duration != AnimatorTransition{}.Duration)
                out << YAML::Key << "Duration" << YAML::Value << transition.Duration;
            if (transition.CanTransitionToSelf)
                out << YAML::Key << "CanTransitionToSelf" << YAML::Value << true;
            if (!transition.Conditions.empty())
            {
                out << YAML::Key << "Conditions" << YAML::Value << YAML::BeginSeq;
                for (const TransitionCondition& condition : transition.Conditions)
                    WriteCondition(out, controller, condition);
                out << YAML::EndSeq;
            }
            out << YAML::EndMap;
        }
    }

    std::string WriteAnimatorController(const AnimatorController& controller)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << ANIMATOR_CONTROLLER_VERSION;

        out << YAML::Key << "Parameters" << YAML::Value << YAML::BeginSeq;
        for (const AnimatorParameter& parameter : controller.Parameters)
            WriteParameter(out, parameter);
        out << YAML::EndSeq;

        out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
        for (const AnimatorState& state : controller.States)
            WriteState(out, state);
        out << YAML::EndSeq;

        out << YAML::Key << "Default" << YAML::Value << StateName(controller, controller.DefaultState);

        out << YAML::Key << "Transitions" << YAML::Value << YAML::BeginSeq;
        for (const AnimatorTransition& transition : controller.Transitions)
            WriteTransition(out, controller, transition);
        out << YAML::EndSeq;

        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }
}
