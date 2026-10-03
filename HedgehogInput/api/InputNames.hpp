#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputCodes.hpp"

#include <optional>
#include <span>
#include <string_view>

// The names an actions file uses for keys, mouse buttons and binding sources. A key's name is its
// HW::Key enumerator ("A", "Num1", "Enter", "LeftShift", "KeypadEnter", ...), a mouse button's its
// HW::MouseButton enumerator ("Left", "Middle", "Button4", ...). Lookups are case-sensitive.
namespace HInput
{
    struct NamedKey
    {
        HW::Key          Key;
        std::string_view Name;
    };

    struct NamedMouseButton
    {
        HW::MouseButton  Button;
        std::string_view Name;
    };

    // Every key and mouse button, each named once, in code order.
    [[nodiscard]] std::span<const NamedKey>         GetKeyNames();
    [[nodiscard]] std::span<const NamedMouseButton> GetMouseButtonNames();

    [[nodiscard]] std::optional<HW::Key>         FindKey(std::string_view name);
    [[nodiscard]] std::optional<HW::MouseButton> FindMouseButton(std::string_view name);

    // The name of a key or button code, or an empty view for a code that names none.
    [[nodiscard]] std::string_view GetKeyName(uint16_t code);
    [[nodiscard]] std::string_view GetMouseButtonName(uint16_t code);
}
