#include "HedgehogInput/api/InputNames.hpp"

#include <algorithm>
#include <array>

namespace HInput
{
    namespace
    {
        constexpr std::array KEY_NAMES = {
            NamedKey{ HW::Key::Space, "Space" },
            NamedKey{ HW::Key::Apostrophe, "Apostrophe" },
            NamedKey{ HW::Key::Comma, "Comma" },
            NamedKey{ HW::Key::Minus, "Minus" },
            NamedKey{ HW::Key::Period, "Period" },
            NamedKey{ HW::Key::Slash, "Slash" },
            NamedKey{ HW::Key::Num0, "Num0" },
            NamedKey{ HW::Key::Num1, "Num1" },
            NamedKey{ HW::Key::Num2, "Num2" },
            NamedKey{ HW::Key::Num3, "Num3" },
            NamedKey{ HW::Key::Num4, "Num4" },
            NamedKey{ HW::Key::Num5, "Num5" },
            NamedKey{ HW::Key::Num6, "Num6" },
            NamedKey{ HW::Key::Num7, "Num7" },
            NamedKey{ HW::Key::Num8, "Num8" },
            NamedKey{ HW::Key::Num9, "Num9" },
            NamedKey{ HW::Key::Semicolon, "Semicolon" },
            NamedKey{ HW::Key::Equal, "Equal" },
            NamedKey{ HW::Key::A, "A" },
            NamedKey{ HW::Key::B, "B" },
            NamedKey{ HW::Key::C, "C" },
            NamedKey{ HW::Key::D, "D" },
            NamedKey{ HW::Key::E, "E" },
            NamedKey{ HW::Key::F, "F" },
            NamedKey{ HW::Key::G, "G" },
            NamedKey{ HW::Key::H, "H" },
            NamedKey{ HW::Key::I, "I" },
            NamedKey{ HW::Key::J, "J" },
            NamedKey{ HW::Key::K, "K" },
            NamedKey{ HW::Key::L, "L" },
            NamedKey{ HW::Key::M, "M" },
            NamedKey{ HW::Key::N, "N" },
            NamedKey{ HW::Key::O, "O" },
            NamedKey{ HW::Key::P, "P" },
            NamedKey{ HW::Key::Q, "Q" },
            NamedKey{ HW::Key::R, "R" },
            NamedKey{ HW::Key::S, "S" },
            NamedKey{ HW::Key::T, "T" },
            NamedKey{ HW::Key::U, "U" },
            NamedKey{ HW::Key::V, "V" },
            NamedKey{ HW::Key::W, "W" },
            NamedKey{ HW::Key::X, "X" },
            NamedKey{ HW::Key::Y, "Y" },
            NamedKey{ HW::Key::Z, "Z" },
            NamedKey{ HW::Key::LeftBracket, "LeftBracket" },
            NamedKey{ HW::Key::Backslash, "Backslash" },
            NamedKey{ HW::Key::RightBracket, "RightBracket" },
            NamedKey{ HW::Key::GraveAccent, "GraveAccent" },
            NamedKey{ HW::Key::World1, "World1" },
            NamedKey{ HW::Key::World2, "World2" },
            NamedKey{ HW::Key::Escape, "Escape" },
            NamedKey{ HW::Key::Enter, "Enter" },
            NamedKey{ HW::Key::Tab, "Tab" },
            NamedKey{ HW::Key::Backspace, "Backspace" },
            NamedKey{ HW::Key::Insert, "Insert" },
            NamedKey{ HW::Key::Delete, "Delete" },
            NamedKey{ HW::Key::Right, "Right" },
            NamedKey{ HW::Key::Left, "Left" },
            NamedKey{ HW::Key::Down, "Down" },
            NamedKey{ HW::Key::Up, "Up" },
            NamedKey{ HW::Key::PageUp, "PageUp" },
            NamedKey{ HW::Key::PageDown, "PageDown" },
            NamedKey{ HW::Key::Home, "Home" },
            NamedKey{ HW::Key::End, "End" },
            NamedKey{ HW::Key::CapsLock, "CapsLock" },
            NamedKey{ HW::Key::ScrollLock, "ScrollLock" },
            NamedKey{ HW::Key::NumLock, "NumLock" },
            NamedKey{ HW::Key::PrintScreen, "PrintScreen" },
            NamedKey{ HW::Key::Pause, "Pause" },
            NamedKey{ HW::Key::F1, "F1" },
            NamedKey{ HW::Key::F2, "F2" },
            NamedKey{ HW::Key::F3, "F3" },
            NamedKey{ HW::Key::F4, "F4" },
            NamedKey{ HW::Key::F5, "F5" },
            NamedKey{ HW::Key::F6, "F6" },
            NamedKey{ HW::Key::F7, "F7" },
            NamedKey{ HW::Key::F8, "F8" },
            NamedKey{ HW::Key::F9, "F9" },
            NamedKey{ HW::Key::F10, "F10" },
            NamedKey{ HW::Key::F11, "F11" },
            NamedKey{ HW::Key::F12, "F12" },
            NamedKey{ HW::Key::F13, "F13" },
            NamedKey{ HW::Key::F14, "F14" },
            NamedKey{ HW::Key::F15, "F15" },
            NamedKey{ HW::Key::F16, "F16" },
            NamedKey{ HW::Key::F17, "F17" },
            NamedKey{ HW::Key::F18, "F18" },
            NamedKey{ HW::Key::F19, "F19" },
            NamedKey{ HW::Key::F20, "F20" },
            NamedKey{ HW::Key::F21, "F21" },
            NamedKey{ HW::Key::F22, "F22" },
            NamedKey{ HW::Key::F23, "F23" },
            NamedKey{ HW::Key::F24, "F24" },
            NamedKey{ HW::Key::F25, "F25" },
            NamedKey{ HW::Key::Keypad0, "Keypad0" },
            NamedKey{ HW::Key::Keypad1, "Keypad1" },
            NamedKey{ HW::Key::Keypad2, "Keypad2" },
            NamedKey{ HW::Key::Keypad3, "Keypad3" },
            NamedKey{ HW::Key::Keypad4, "Keypad4" },
            NamedKey{ HW::Key::Keypad5, "Keypad5" },
            NamedKey{ HW::Key::Keypad6, "Keypad6" },
            NamedKey{ HW::Key::Keypad7, "Keypad7" },
            NamedKey{ HW::Key::Keypad8, "Keypad8" },
            NamedKey{ HW::Key::Keypad9, "Keypad9" },
            NamedKey{ HW::Key::KeypadDecimal, "KeypadDecimal" },
            NamedKey{ HW::Key::KeypadDivide, "KeypadDivide" },
            NamedKey{ HW::Key::KeypadMultiply, "KeypadMultiply" },
            NamedKey{ HW::Key::KeypadSubtract, "KeypadSubtract" },
            NamedKey{ HW::Key::KeypadAdd, "KeypadAdd" },
            NamedKey{ HW::Key::KeypadEnter, "KeypadEnter" },
            NamedKey{ HW::Key::KeypadEqual, "KeypadEqual" },
            NamedKey{ HW::Key::LeftShift, "LeftShift" },
            NamedKey{ HW::Key::LeftControl, "LeftControl" },
            NamedKey{ HW::Key::LeftAlt, "LeftAlt" },
            NamedKey{ HW::Key::LeftSuper, "LeftSuper" },
            NamedKey{ HW::Key::RightShift, "RightShift" },
            NamedKey{ HW::Key::RightControl, "RightControl" },
            NamedKey{ HW::Key::RightAlt, "RightAlt" },
            NamedKey{ HW::Key::RightSuper, "RightSuper" },
            NamedKey{ HW::Key::Menu, "Menu" },
        };

        constexpr std::array MOUSE_BUTTON_NAMES = {
            NamedMouseButton{ HW::MouseButton::Left, "Left" },
            NamedMouseButton{ HW::MouseButton::Right, "Right" },
            NamedMouseButton{ HW::MouseButton::Middle, "Middle" },
            NamedMouseButton{ HW::MouseButton::Button4, "Button4" },
            NamedMouseButton{ HW::MouseButton::Button5, "Button5" },
            NamedMouseButton{ HW::MouseButton::Button6, "Button6" },
            NamedMouseButton{ HW::MouseButton::Button7, "Button7" },
            NamedMouseButton{ HW::MouseButton::Button8, "Button8" },
        };
    }

    std::span<const NamedKey> GetKeyNames()
    {
        return KEY_NAMES;
    }

    std::span<const NamedMouseButton> GetMouseButtonNames()
    {
        return MOUSE_BUTTON_NAMES;
    }

    std::optional<HW::Key> FindKey(std::string_view name)
    {
        const auto found = std::find_if(KEY_NAMES.begin(), KEY_NAMES.end(),
                                        [&](const NamedKey& key) { return key.Name == name; });
        return found != KEY_NAMES.end() ? std::optional(found->Key) : std::nullopt;
    }

    std::optional<HW::MouseButton> FindMouseButton(std::string_view name)
    {
        const auto found = std::find_if(MOUSE_BUTTON_NAMES.begin(), MOUSE_BUTTON_NAMES.end(),
                                        [&](const NamedMouseButton& button) { return button.Name == name; });
        return found != MOUSE_BUTTON_NAMES.end() ? std::optional(found->Button) : std::nullopt;
    }

    std::string_view GetKeyName(uint16_t code)
    {
        const auto found = std::find_if(KEY_NAMES.begin(), KEY_NAMES.end(),
                                        [&](const NamedKey& key) { return static_cast<uint16_t>(key.Key) == code; });
        return found != KEY_NAMES.end() ? found->Name : std::string_view{};
    }

    std::string_view GetMouseButtonName(uint16_t code)
    {
        const auto found = std::find_if(MOUSE_BUTTON_NAMES.begin(), MOUSE_BUTTON_NAMES.end(),
                                        [&](const NamedMouseButton& button)
                                        { return static_cast<uint16_t>(button.Button) == code; });
        return found != MOUSE_BUTTON_NAMES.end() ? found->Name : std::string_view{};
    }
}
