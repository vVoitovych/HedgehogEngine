#include "HedgehogInput/api/DefaultInputActions.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputCodes.hpp"

namespace HInput
{
    namespace
    {
        constexpr float EDITOR_LOOK_DEADZONE = 2.0f; // pixels, as the flycam has always ignored

        InputBinding Key(HW::Key key)
        {
            return { BindingSource::Key, static_cast<uint16_t>(key) };
        }

        InputBinding Button(HW::MouseButton button)
        {
            return { BindingSource::MouseButton, static_cast<uint16_t>(button) };
        }

        InputBinding KeyAxis(HW::Key negative, HW::Key positive)
        {
            return { BindingSource::KeyAxis, static_cast<uint16_t>(positive), static_cast<uint16_t>(negative) };
        }

        constexpr float STICK_DEADZONE = 0.2f;

        InputBinding Pad(HW::GamepadButton button)
        {
            return { BindingSource::GamepadButton, static_cast<uint16_t>(button) };
        }

        // A stick pushed one way: scale -1 for up (y is down) or left.
        InputBinding Stick(HW::GamepadAxis axis, float scale)
        {
            InputBinding binding;
            binding.Source   = BindingSource::GamepadAxis;
            binding.Code     = static_cast<uint16_t>(axis);
            binding.Scale    = scale;
            binding.Deadzone = STICK_DEADZONE;
            return binding;
        }

        InputBinding Pointer(BindingSource axis)
        {
            InputBinding binding;
            binding.Source   = axis;
            binding.Deadzone = EDITOR_LOOK_DEADZONE;
            return binding;
        }
    }

    InputActionSet MakeDefaultInputActions()
    {
        InputActionSet set;
        set.Game.Actions = {
            { "UiNavigateUp", { Key(HW::Key::Up), Pad(HW::GamepadButton::DpadUp), Stick(HW::GamepadAxis::LeftY, -1.0f) } },
            { "UiNavigateDown", { Key(HW::Key::Down), Pad(HW::GamepadButton::DpadDown), Stick(HW::GamepadAxis::LeftY, 1.0f) } },
            { "UiNavigateLeft", { Key(HW::Key::Left), Pad(HW::GamepadButton::DpadLeft), Stick(HW::GamepadAxis::LeftX, -1.0f) } },
            { "UiNavigateRight", { Key(HW::Key::Right), Pad(HW::GamepadButton::DpadRight), Stick(HW::GamepadAxis::LeftX, 1.0f) } },
            { "UiSubmit", { Key(HW::Key::Enter), Key(HW::Key::KeypadEnter), Key(HW::Key::Space), Pad(HW::GamepadButton::A) } },
            { "UiPointerPress", { Button(HW::MouseButton::Left) } },
        };
        set.Editor.Actions = {
            { "EditorCameraForward", { KeyAxis(HW::Key::S, HW::Key::W) } },
            { "EditorCameraRight", { KeyAxis(HW::Key::A, HW::Key::D) } },
            { "EditorCameraUp", { KeyAxis(HW::Key::Q, HW::Key::E) } },
            { "EditorCameraLookX", { Pointer(BindingSource::PointerDeltaX) } },
            { "EditorCameraLookY", { Pointer(BindingSource::PointerDeltaY) } },
            { "EditorCameraLookHold", { Button(HW::MouseButton::Left) } },
            { "EditorCameraPanHold", { Button(HW::MouseButton::Right), Button(HW::MouseButton::Middle) } },
            { "EditorCameraZoom", { InputBinding{ BindingSource::ScrollY } } },
        };
        return set;
    }
}
