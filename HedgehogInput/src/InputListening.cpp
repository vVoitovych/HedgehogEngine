#include "HedgehogInput/api/InputListening.hpp"

#include "HedgehogInput/api/ActionState.hpp"
#include "HedgehogInput/api/InputNames.hpp"

#include <cmath>

namespace HInput
{
    std::optional<InputPress> FindNewPress(const HW::RawInput& previous, const HW::RawInput& current,
                                           BindingSource source)
    {
        switch (source)
        {
        case BindingSource::Key:
        case BindingSource::KeyAxis:
            for (size_t key = 0; key < HW::KEY_COUNT; ++key)
            {
                if (current.Keys[key] && !previous.Keys[key] && !GetKeyName(static_cast<uint16_t>(key)).empty())
                {
                    return InputPress{ static_cast<uint16_t>(key), 1.0f };
                }
            }
            break;
        case BindingSource::MouseButton:
            for (size_t button = 0; button < HW::MOUSE_BUTTON_COUNT; ++button)
            {
                if (current.MouseButtons[button] && !previous.MouseButtons[button])
                {
                    return InputPress{ static_cast<uint16_t>(button), 1.0f };
                }
            }
            break;
        case BindingSource::GamepadButton:
            for (size_t button = 0; button < HW::GAMEPAD_BUTTON_COUNT; ++button)
            {
                if (current.Gamepad.Buttons[button] && !previous.Gamepad.Buttons[button])
                {
                    return InputPress{ static_cast<uint16_t>(button), 1.0f };
                }
            }
            break;
        case BindingSource::GamepadAxis:
            for (size_t axis = 0; axis < HW::GAMEPAD_AXIS_COUNT; ++axis)
            {
                const float now = current.Gamepad.Axes[axis];
                if (std::abs(now) >= PRESS_THRESHOLD && std::abs(previous.Gamepad.Axes[axis]) < PRESS_THRESHOLD)
                {
                    return InputPress{ static_cast<uint16_t>(axis), now < 0.0f ? -1.0f : 1.0f };
                }
            }
            break;
        default:
            break;
        }
        return std::nullopt;
    }
}
