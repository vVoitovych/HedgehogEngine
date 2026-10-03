#include "HedgehogInput/api/InputActionFile.hpp"

#include "HedgehogInput/api/InputNames.hpp"

#include <yaml-cpp/yaml.h>

namespace HInput
{
    namespace
    {
        const char* AxisName(BindingSource source)
        {
            return source == BindingSource::PointerDeltaX || source == BindingSource::ScrollX ? "X" : "Y";
        }

        void WriteBinding(YAML::Emitter& out, const InputBinding& binding)
        {
            out << YAML::BeginMap;
            switch (binding.Source)
            {
            case BindingSource::Key:
                out << YAML::Key << "Key" << YAML::Value << std::string(GetKeyName(binding.Code));
                break;
            case BindingSource::MouseButton:
                out << YAML::Key << "MouseButton" << YAML::Value << std::string(GetMouseButtonName(binding.Code));
                break;
            case BindingSource::KeyAxis:
                out << YAML::Key << "KeyAxis" << YAML::Value << YAML::Flow << YAML::BeginMap
                    << YAML::Key << "Negative" << YAML::Value << std::string(GetKeyName(binding.NegativeCode))
                    << YAML::Key << "Positive" << YAML::Value << std::string(GetKeyName(binding.Code))
                    << YAML::EndMap;
                break;
            case BindingSource::PointerDeltaX:
            case BindingSource::PointerDeltaY:
                out << YAML::Key << "PointerDelta" << YAML::Value << AxisName(binding.Source);
                break;
            case BindingSource::ScrollX:
            case BindingSource::ScrollY:
                out << YAML::Key << "Scroll" << YAML::Value << AxisName(binding.Source);
                break;
            }
            if (binding.Scale != 1.0f)
                out << YAML::Key << "Scale" << YAML::Value << binding.Scale;
            if (binding.Deadzone != 0.0f)
                out << YAML::Key << "Deadzone" << YAML::Value << binding.Deadzone;
            out << YAML::EndMap;
        }

        void WriteMap(YAML::Emitter& out, const char* name, const InputActionMap& map)
        {
            out << YAML::Key << name << YAML::Value << YAML::BeginMap;
            for (const InputAction& action : map.Actions)
            {
                out << YAML::Key << action.Name << YAML::Value << YAML::BeginSeq;
                for (const InputBinding& binding : action.Bindings)
                    WriteBinding(out, binding);
                out << YAML::EndSeq;
            }
            out << YAML::EndMap;
        }
    }

    std::string WriteInputActions(const InputActionSet& set)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Version" << YAML::Value << INPUT_ACTIONS_VERSION;
        WriteMap(out, "Game", set.Game);
        WriteMap(out, "Editor", set.Editor);
        out << YAML::EndMap;
        return std::string(out.c_str()) + "\n";
    }
}
