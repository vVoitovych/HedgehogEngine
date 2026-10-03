#include "HedgehogInput/api/ActionState.hpp"

#include <algorithm>
#include <cmath>

namespace HInput
{
    namespace
    {
        bool KeyDown(const HW::RawInput& input, uint16_t code)
        {
            return code < HW::KEY_COUNT && input.Keys[code];
        }

        bool ButtonDown(const HW::RawInput& input, uint16_t code)
        {
            return code < HW::MOUSE_BUTTON_COUNT && input.MouseButtons[code];
        }

        // A pointer or scroll move, zero when within the deadzone.
        float Movement(float value, const InputBinding& binding)
        {
            return std::abs(value) <= binding.Deadzone ? 0.0f : value * binding.Scale;
        }

        // A stick or trigger value outside the deadzone, rescaled so the deadzone's edge is 0 and full
        // tilt is 1, then scaled and clamped.
        float Axis(const HW::RawInput& input, const InputBinding& binding)
        {
            if (!input.Gamepad.Connected || binding.Code >= HW::GAMEPAD_AXIS_COUNT)
                return 0.0f;
            const float value     = input.Gamepad.Axes[binding.Code];
            const float magnitude = std::abs(value);
            if (magnitude <= binding.Deadzone || binding.Deadzone >= 1.0f)
                return 0.0f;
            const float rescaled = std::copysign((magnitude - binding.Deadzone) / (1.0f - binding.Deadzone), value);
            return std::clamp(rescaled * binding.Scale, -1.0f, 1.0f);
        }

        // One binding's value this frame.
        float Evaluate(const InputBinding& binding, const HW::RawInput& input)
        {
            switch (binding.Source)
            {
            case BindingSource::Key:
                return KeyDown(input, binding.Code) ? std::clamp(binding.Scale, -1.0f, 1.0f) : 0.0f;
            case BindingSource::MouseButton:
                return ButtonDown(input, binding.Code) ? std::clamp(binding.Scale, -1.0f, 1.0f) : 0.0f;
            case BindingSource::KeyAxis:
            {
                const float axis = (KeyDown(input, binding.Code) ? 1.0f : 0.0f) -
                                   (KeyDown(input, binding.NegativeCode) ? 1.0f : 0.0f);
                return std::clamp(axis * binding.Scale, -1.0f, 1.0f);
            }
            case BindingSource::PointerDeltaX: return Movement(input.CursorDelta.x(), binding);
            case BindingSource::PointerDeltaY: return Movement(input.CursorDelta.y(), binding);
            case BindingSource::ScrollX:       return Movement(input.ScrollDelta.x(), binding);
            case BindingSource::ScrollY:       return Movement(input.ScrollDelta.y(), binding);
            case BindingSource::GamepadButton:
                return input.Gamepad.Connected && binding.Code < HW::GAMEPAD_BUTTON_COUNT && input.Gamepad.Buttons[binding.Code]
                           ? std::clamp(binding.Scale, -1.0f, 1.0f)
                           : 0.0f;
            case BindingSource::GamepadAxis:   return Axis(input, binding);
            }
            return 0.0f;
        }

        bool InRange(const ActionState& state, size_t action)
        {
            return action < state.Actions.size();
        }

        bool Live(const ActionState& state, size_t action)
        {
            return InRange(state, action) && !state.Actions[action].Consumed;
        }
    }

    std::optional<size_t> FindAction(const InputActionMap& map, std::string_view name)
    {
        for (size_t i = 0; i < map.Actions.size(); ++i)
        {
            if (map.Actions[i].Name == name)
                return i;
        }
        return std::nullopt;
    }

    void UpdateActionState(const InputActionMap& map, const HW::RawInput& input, ActionState& state)
    {
        if (state.Actions.size() != map.Actions.size())
            ResetActionState(state, map);

        for (size_t i = 0; i < map.Actions.size(); ++i)
        {
            float value = 0.0f;
            for (const InputBinding& binding : map.Actions[i].Bindings)
            {
                const float bindingValue = Evaluate(binding, input);
                if (std::abs(bindingValue) > std::abs(value))
                    value = bindingValue;
            }

            ActionValue& action = state.Actions[i];
            const bool   wasDown = action.Down;
            action.Value    = value;
            action.Down     = value >= PRESS_THRESHOLD;
            action.Pressed  = action.Down && !wasDown;
            action.Released = !action.Down && wasDown;
            action.Consumed = false;
        }

        state.Pointer.Position = input.CursorPosition;
        state.Pointer.Delta    = input.CursorDelta;
        state.Pointer.Inside   = input.CursorInside;
    }

    void ResetActionState(ActionState& state, const InputActionMap& map)
    {
        state.Actions.assign(map.Actions.size(), ActionValue{});
        state.Pointer = PointerState{};
    }

    void ConsumeAction(ActionState& state, size_t action)
    {
        if (InRange(state, action))
            state.Actions[action].Consumed = true;
    }

    bool IsActionDown(const ActionState& state, size_t action)
    {
        return Live(state, action) && state.Actions[action].Down;
    }

    bool WasActionPressed(const ActionState& state, size_t action)
    {
        return Live(state, action) && state.Actions[action].Pressed;
    }

    bool WasActionReleased(const ActionState& state, size_t action)
    {
        return Live(state, action) && state.Actions[action].Released;
    }

    float GetActionValue(const ActionState& state, size_t action)
    {
        return Live(state, action) ? state.Actions[action].Value : 0.0f;
    }
}
