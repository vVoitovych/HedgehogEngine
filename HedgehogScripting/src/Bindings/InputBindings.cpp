#include "Bindings.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputActionMap.hpp"
#include "HedgehogInput/api/InputListening.hpp"
#include "HedgehogInput/api/InputNames.hpp"
#include "HedgehogInput/api/InputOverrides.hpp"

#include "Logger/api/Logger.hpp"

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HInput::BindingSource;

        // In BindingSource's order.
        constexpr std::array<std::string_view, 9> SOURCE_NAMES = { "Key",           "MouseButton",   "KeyAxis",
                                                                   "PointerDeltaX", "PointerDeltaY", "ScrollX",
                                                                   "ScrollY",       "GamepadButton", "GamepadAxis" };

        // The index of the game action called name; an unknown name is a script error.
        size_t ActionIndex(const HedgehogEngine::EngineContext& context, const std::string& name)
        {
            const std::optional<size_t> index = HInput::FindAction(context.GetInputActions().Game, name);
            if (!index)
                throw std::runtime_error("Input: no action '" + name + "' in " +
                                         HedgehogEngine::EngineContext::INPUT_ACTIONS_PATH);
            return *index;
        }

        const HInput::InputAction& FindGameAction(const HedgehogEngine::EngineContext& context, const std::string& name)
        {
            return context.GetInputActions().Game.Actions[ActionIndex(context, name)];
        }

        // A Lua binding index (from 1) of action as a C++ one; out of range is a script error.
        size_t BindingIndex(const HInput::InputAction& action, int64_t index, const char* call)
        {
            if (index < 1 || static_cast<size_t>(index) > action.Bindings.size())
                throw std::runtime_error(std::string(call) + ": " + action.Name + " has no binding " +
                                         std::to_string(index) + " (it has " +
                                         std::to_string(action.Bindings.size()) + ")");
            return static_cast<size_t>(index - 1);
        }

        bool HasCode(BindingSource source)
        {
            return source == BindingSource::Key || source == BindingSource::MouseButton ||
                   source == BindingSource::KeyAxis || source == BindingSource::GamepadButton ||
                   source == BindingSource::GamepadAxis;
        }

        std::string_view CodeName(BindingSource source, uint16_t code)
        {
            switch (source)
            {
            case BindingSource::Key:
            case BindingSource::KeyAxis:       return HInput::GetKeyName(code);
            case BindingSource::MouseButton:   return HInput::GetMouseButtonName(code);
            case BindingSource::GamepadButton: return HInput::GetGamepadButtonName(code);
            case BindingSource::GamepadAxis:   return HInput::GetGamepadAxisName(code);
            default:                           return {};
            }
        }

        std::optional<uint16_t> FindCode(BindingSource source, std::string_view name)
        {
            switch (source)
            {
            case BindingSource::Key:
            case BindingSource::KeyAxis:
                if (const auto key = HInput::FindKey(name))
                    return static_cast<uint16_t>(*key);
                return std::nullopt;
            case BindingSource::MouseButton:
                if (const auto button = HInput::FindMouseButton(name))
                    return static_cast<uint16_t>(*button);
                return std::nullopt;
            case BindingSource::GamepadButton:
                if (const auto button = HInput::FindGamepadButton(name))
                    return static_cast<uint16_t>(*button);
                return std::nullopt;
            case BindingSource::GamepadAxis:
                if (const auto axis = HInput::FindGamepadAxis(name))
                    return static_cast<uint16_t>(*axis);
                return std::nullopt;
            default: return std::nullopt;
            }
        }

        // { source, name (a code source's), negative (KeyAxis), scale, deadzone }.
        sol::table ToTable(sol::state_view lua, const HInput::InputBinding& binding)
        {
            sol::table table = lua.create_table();
            table["source"]  = std::string(SOURCE_NAMES[static_cast<size_t>(binding.Source)]);
            if (HasCode(binding.Source))
                table["name"] = std::string(CodeName(binding.Source, binding.Code));
            if (binding.Source == BindingSource::KeyAxis)
                table["negative"] = std::string(HInput::GetKeyName(binding.NegativeCode));
            table["scale"]    = binding.Scale;
            table["deadzone"] = binding.Deadzone;
            return table;
        }

        std::string ReadName(const sol::table& table, const char* key, const char* call)
        {
            const sol::object value = table[key];
            if (value.get_type() != sol::type::string)
                throw std::runtime_error(std::string(call) + ": the binding's " + key + " must be a string");
            return value.as<std::string>();
        }

        float ReadNumber(const sol::table& table, const char* key, float fallback, const char* call)
        {
            const sol::object value = table[key];
            if (value.get_type() == sol::type::lua_nil)
                return fallback;
            if (value.get_type() != sol::type::number || !std::isfinite(value.as<double>()))
                throw std::runtime_error(std::string(call) + ": the binding's " + key + " must be a finite number");
            return value.as<float>();
        }

        HInput::InputBinding FromTable(const sol::object& object, const char* call)
        {
            if (object.get_type() != sol::type::table)
                throw std::runtime_error(std::string(call) + ": the binding must be a table");
            const sol::table table  = object.as<sol::table>();
            const std::string source = ReadName(table, "source", call);

            HInput::InputBinding binding;
            size_t               index = 0;
            while (index < SOURCE_NAMES.size() && SOURCE_NAMES[index] != source)
                ++index;
            if (index == SOURCE_NAMES.size())
                throw std::runtime_error(std::string(call) + ": the binding's source '" + source +
                                         "' is not a binding source");
            binding.Source = static_cast<BindingSource>(index);
            if (HasCode(binding.Source))
            {
                const std::string           name = ReadName(table, "name", call);
                const std::optional<uint16_t> code = FindCode(binding.Source, name);
                if (!code)
                    throw std::runtime_error(std::string(call) + ": '" + name + "' names no " + source + " control");
                binding.Code = *code;
            }
            if (binding.Source == BindingSource::KeyAxis)
            {
                const std::string           name = ReadName(table, "negative", call);
                const std::optional<HW::Key> key  = HInput::FindKey(name);
                if (!key)
                    throw std::runtime_error(std::string(call) + ": '" + name + "' names no key");
                binding.NegativeCode = static_cast<uint16_t>(*key);
            }
            binding.Scale    = ReadNumber(table, "scale", 1.0f, call);
            binding.Deadzone = ReadNumber(table, "deadzone", 0.0f, call);
            return binding;
        }

        // Rebinding changes the player's bindings, so it waits for Play (a script's top level also runs
        // when the editor describes it); the first refused call says why.
        struct RebindAccess
        {
            HedgehogEngine::EngineContext& Context;
            InputListen&                   Listen;
            bool                           Warned = false;

            bool InPlay(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Input bindings change only in Play mode; ", call, " did nothing.");
                Warned = true;
                return false;
            }

            void SetBinding(const std::string& action, size_t index, const HInput::InputBinding& binding)
            {
                HInput::InputActionMap overrides = Context.GetInputOverrides();
                if (!HInput::SetActionBinding(overrides, Context.GetProjectInputActions().Game, action, index, binding))
                    throw std::runtime_error("Input: " + action + " cannot be rebound");
                Context.SetInputOverrides(std::move(overrides));
            }
        };
    }

    void RegisterInput(sol::state& lua, HedgehogEngine::EngineContext& context, InputListen& listen)
    {
        // Every function reads the engine's game action state when called, so a reloaded actions
        // file or the next frame's input is seen at once.
        sol::table input = lua.create_named_table("Input");

        input.set_function("isDown", [&context](const std::string& action)
                           { return HInput::IsActionDown(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("wasPressed", [&context](const std::string& action)
                           { return HInput::WasActionPressed(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("wasReleased", [&context](const std::string& action)
                           { return HInput::WasActionReleased(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("value", [&context](const std::string& action)
                           { return HInput::GetActionValue(context.GetGameActionState(), ActionIndex(context, action)); });
        input.set_function("consume", [&context](const std::string& action)
                           { HInput::ConsumeAction(context.GetGameActionState(), ActionIndex(context, action)); });

        input.set_function("pointerPosition", [&context]()
        {
            const HM::Vector2& position = context.GetGameActionState().Pointer.Position;
            return std::make_tuple(position.x(), position.y());
        });
        input.set_function("pointerDelta", [&context]()
        {
            const HM::Vector2& delta = context.GetGameActionState().Pointer.Delta;
            return std::make_tuple(delta.x(), delta.y());
        });
        input.set_function("isPointerInside", [&context]() { return context.GetGameActionState().Pointer.Inside; });

        // Rebinding: the player's overrides of the Game actions (EngineContext::SetInputOverrides).
        const auto access = std::make_shared<RebindAccess>(RebindAccess{ context, listen });

        input.set_function("getBindings", [&context](const std::string& action, sol::this_state state)
        {
            sol::table bindings = sol::state_view(state).create_table();
            for (const HInput::InputBinding& binding : FindGameAction(context, action).Bindings)
                bindings.add(ToTable(state, binding));
            return bindings;
        });
        input.set_function("setBinding", [access](const std::string& action, int64_t index, sol::object binding)
        {
            const size_t               slot  = BindingIndex(FindGameAction(access->Context, action), index,
                                                            "Input.setBinding");
            const HInput::InputBinding value = FromTable(binding, "Input.setBinding");
            if (access->InPlay("Input.setBinding"))
                access->SetBinding(action, slot, value);
        });
        input.set_function("resetBindings", [access](sol::optional<std::string> action)
        {
            if (action)
                (void)ActionIndex(access->Context, *action);
            if (!access->InPlay("Input.resetBindings"))
                return;
            HInput::InputActionMap overrides = access->Context.GetInputOverrides();
            if (action)
                (void)HInput::ResetActionBindings(overrides, *action);
            else
                overrides.Actions.clear();
            access->Context.SetInputOverrides(std::move(overrides));
        });
        input.set_function("listen", [access](const std::string& action, int64_t index)
        {
            const HInput::InputAction& game = FindGameAction(access->Context, action);
            const size_t               slot = BindingIndex(game, index, "Input.listen");
            if (!HasCode(game.Bindings[slot].Source))
                throw std::runtime_error("Input.listen: " + action + "'s binding " + std::to_string(index) +
                                         " is a pointer or scroll binding, which cannot be listened for");
            if (!access->InPlay("Input.listen"))
                return;
            access->Listen = InputListen{ true, action, slot, access->Context.GetLastGameInput() };
        });
        input.set_function("isListening", [&listen]() { return listen.Active; });
        input.set_function("saveBindings", [access]()
                           { return access->InPlay("Input.saveBindings") && access->Context.SaveInputOverrides(); });
    }

    std::optional<sol::table> UpdateInputListen(sol::state& lua, HedgehogEngine::EngineContext& context,
                                                InputListen& listen)
    {
        if (!listen.Active)
            return std::nullopt;

        const HW::RawInput&               current = context.GetLastGameInput();
        const HInput::InputActionMap&     game    = context.GetInputActions().Game;
        const std::optional<size_t>       action  = HInput::FindAction(game, listen.Action);
        std::optional<HInput::InputPress> press;
        bool                              cancelled = !action || listen.Index >= game.Actions[*action].Bindings.size();
        if (!cancelled)
        {
            const size_t escape = static_cast<size_t>(HW::Key::Escape);
            cancelled           = current.Keys[escape] && !listen.Previous.Keys[escape];
        }
        if (!cancelled)
            press = HInput::FindNewPress(listen.Previous, current,
                                         game.Actions[*action].Bindings[listen.Index].Source);
        listen.Previous = current;
        if (!cancelled && !press)
            return std::nullopt;

        listen.Active      = false;
        sol::table payload = lua.create_table_with("action", listen.Action, "index", listen.Index + 1);
        if (cancelled)
        {
            payload["cancelled"] = true;
            return payload;
        }

        HInput::InputBinding binding = game.Actions[*action].Bindings[listen.Index];
        binding.Code                 = press->Code;
        if (binding.Source == BindingSource::GamepadAxis)
            binding.Scale = std::abs(binding.Scale) * press->Direction;
        HInput::InputActionMap overrides = context.GetInputOverrides();
        if (HInput::SetActionBinding(overrides, context.GetProjectInputActions().Game, listen.Action, listen.Index,
                                     binding))
            context.SetInputOverrides(std::move(overrides));
        payload["binding"] = ToTable(lua, binding);
        return payload;
    }

    void ConsumeAllInput(HedgehogEngine::EngineContext& context)
    {
        HInput::ActionState& state = context.GetGameActionState();
        for (size_t i = 0; i < context.GetInputActions().Game.Actions.size(); ++i)
            HInput::ConsumeAction(state, i);
    }
}
