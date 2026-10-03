#include "HedgehogInput/api/InputActionFile.hpp"

#include "HedgehogInput/api/InputNames.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>

namespace HInput
{
    namespace
    {
        constexpr std::array SOURCE_KEYS = { "Key", "MouseButton", "KeyAxis", "PointerDelta", "Scroll" };

        // A parse failure, carrying the message up to ParseInputActions.
        struct ParseError
        {
            std::string Message;
        };

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
                return node.as<float>();
            }
            catch (const YAML::Exception&)
            {
                throw ParseError{ where + ": '" + text + "' is not a number" };
            }
        }

        uint16_t KeyCode(const YAML::Node& node, const std::string& where)
        {
            const std::string name = Scalar(node, where);
            const std::optional<HW::Key> key = FindKey(name);
            if (!key)
                throw ParseError{ where + ": unknown key '" + name + "'" };
            return static_cast<uint16_t>(*key);
        }

        // "X" or "Y" for PointerDelta and Scroll.
        bool IsX(const YAML::Node& node, const std::string& where)
        {
            const std::string axis = Scalar(node, where);
            if (axis != "X" && axis != "Y")
                throw ParseError{ where + ": axis '" + axis + "' is not X or Y" };
            return axis == "X";
        }

        void ReadKeyAxis(const YAML::Node& node, InputBinding& binding, const std::string& where)
        {
            if (!node.IsMap())
                throw ParseError{ where + ": KeyAxis needs Negative and Positive keys" };
            for (const auto& entry : node)
            {
                const std::string field = entry.first.Scalar();
                if (field != "Negative" && field != "Positive")
                    throw ParseError{ where + ": unknown KeyAxis field '" + field + "'" };
            }
            if (!node["Negative"] || !node["Positive"])
                throw ParseError{ where + ": KeyAxis needs Negative and Positive keys" };
            binding.NegativeCode = KeyCode(node["Negative"], where);
            binding.Code         = KeyCode(node["Positive"], where);
        }

        InputBinding ReadBinding(const YAML::Node& node, const std::string& where)
        {
            if (!node.IsMap())
                throw ParseError{ where + ": a binding is a map with one source" };

            InputBinding binding;
            int          sources = 0;
            for (const auto& entry : node)
            {
                const std::string field = entry.first.Scalar();
                const YAML::Node& value = entry.second;
                if (field == "Scale")
                    binding.Scale = Number(value, where);
                else if (field == "Deadzone")
                    binding.Deadzone = Number(value, where);
                else if (std::find(SOURCE_KEYS.begin(), SOURCE_KEYS.end(), field) == SOURCE_KEYS.end())
                    throw ParseError{ where + ": unknown field '" + field + "'" };
                else if (++sources > 1)
                    throw ParseError{ where + ": more than one source" };
                else if (field == "Key")
                {
                    binding.Source = BindingSource::Key;
                    binding.Code   = KeyCode(value, where);
                }
                else if (field == "MouseButton")
                {
                    const std::string                    name   = Scalar(value, where);
                    const std::optional<HW::MouseButton> button = FindMouseButton(name);
                    if (!button)
                        throw ParseError{ where + ": unknown mouse button '" + name + "'" };
                    binding.Source = BindingSource::MouseButton;
                    binding.Code   = static_cast<uint16_t>(*button);
                }
                else if (field == "KeyAxis")
                {
                    binding.Source = BindingSource::KeyAxis;
                    ReadKeyAxis(value, binding, where);
                }
                else if (field == "PointerDelta")
                    binding.Source = IsX(value, where) ? BindingSource::PointerDeltaX : BindingSource::PointerDeltaY;
                else
                    binding.Source = IsX(value, where) ? BindingSource::ScrollX : BindingSource::ScrollY;
            }
            if (sources == 0)
                throw ParseError{ where + ": no source (Key, MouseButton, KeyAxis, PointerDelta or Scroll)" };
            return binding;
        }

        InputActionMap ReadMap(const YAML::Node& node, const std::string& mapName)
        {
            InputActionMap map;
            if (!node || node.IsNull())
                return map;
            if (!node.IsMap())
                throw ParseError{ mapName + ": expected a map of actions" };

            for (const auto& entry : node)
            {
                const std::string name  = entry.first.Scalar();
                const std::string where = mapName + ": action '" + name + "'";

                InputAction action;
                action.Name = name;
                const YAML::Node& bindings = entry.second;
                if (!bindings.IsNull())
                {
                    if (!bindings.IsSequence())
                        throw ParseError{ where + ": expected a list of bindings" };
                    for (size_t i = 0; i < bindings.size(); ++i)
                        action.Bindings.push_back(ReadBinding(bindings[i], where + ", binding " + std::to_string(i)));
                }
                map.Actions.push_back(std::move(action));
            }
            return map;
        }
    }

    InputActionParseResult ParseInputActions(std::string_view text)
    {
        InputActionParseResult result;
        try
        {
            const YAML::Node root = YAML::Load(std::string(text));
            if (!root.IsMap())
                throw ParseError{ "the file is not a map of Version, Game and Editor" };
            for (const auto& entry : root)
            {
                const std::string key = entry.first.Scalar();
                if (key != "Version" && key != "Game" && key != "Editor")
                    throw ParseError{ "unknown top-level key '" + key + "'" };
            }
            if (!root["Version"])
                throw ParseError{ "Version is missing" };
            const std::string version = Scalar(root["Version"], "Version");
            if (version != std::to_string(INPUT_ACTIONS_VERSION))
                throw ParseError{ "Version " + version + " is not supported (this build reads version " +
                                  std::to_string(INPUT_ACTIONS_VERSION) + ")" };

            InputActionSet set;
            set.Game   = ReadMap(root["Game"], "Game");
            set.Editor = ReadMap(root["Editor"], "Editor");
            result.Actions = std::move(set);
        }
        catch (const ParseError& error)
        {
            result.Error = error.Message;
        }
        catch (const YAML::Exception& error)
        {
            result.Error = std::string("not valid YAML: ") + error.what();
        }
        return result;
    }
}
