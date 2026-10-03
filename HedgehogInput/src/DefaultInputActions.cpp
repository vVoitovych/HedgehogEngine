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
            { "UiNavigateUp", { Key(HW::Key::Up) } },
            { "UiNavigateDown", { Key(HW::Key::Down) } },
            { "UiNavigateLeft", { Key(HW::Key::Left) } },
            { "UiNavigateRight", { Key(HW::Key::Right) } },
            { "UiSubmit", { Key(HW::Key::Enter), Key(HW::Key::KeypadEnter), Key(HW::Key::Space) } },
            { "UiPointerPress", { Button(HW::MouseButton::Left) } },
        };
        set.Editor.Actions = {
            { "EditorCameraForward", { KeyAxis(HW::Key::S, HW::Key::W) } },
            { "EditorCameraRight", { KeyAxis(HW::Key::A, HW::Key::D) } },
            { "EditorCameraUp", { KeyAxis(HW::Key::Q, HW::Key::E) } },
            { "EditorCameraLookX", { Pointer(BindingSource::PointerDeltaX) } },
            { "EditorCameraLookY", { Pointer(BindingSource::PointerDeltaY) } },
            { "EditorCameraLookHold",
              { Button(HW::MouseButton::Left), Button(HW::MouseButton::Right), Button(HW::MouseButton::Middle) } },
        };
        return set;
    }
}
