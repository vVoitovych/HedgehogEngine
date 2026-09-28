#pragma once

#include "imgui.h"

#include <cstdint>

// The editor's charcoal and silver palette. Every UI colour comes from here; colours that carry
// meaning (errors, warnings, log levels, asset types) stay with the code that uses them.
namespace Editor::Theme
{
    [[nodiscard]] constexpr ImVec4 FromHex(uint32_t rgb, float alpha = 1.0f)
    {
        return { static_cast<float>((rgb >> 16) & 0xFF) / 255.0f,
                 static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                 static_cast<float>(rgb & 0xFF) / 255.0f,
                 alpha };
    }

    [[nodiscard]] constexpr ImVec4 WithAlpha(const ImVec4& color, float alpha)
    {
        return { color.x, color.y, color.z, alpha };
    }

    inline constexpr ImVec4 MAIN_BG    = FromHex(0x17191B);
    inline constexpr ImVec4 PANEL      = FromHex(0x24272A);
    inline constexpr ImVec4 FRAME      = FromHex(0x303438); // inputs and inactive controls
    inline constexpr ImVec4 HOVER      = FromHex(0x42474B);
    inline constexpr ImVec4 ACCENT     = FromHex(0xAEB5BA); // selected / active; use sparingly
    inline constexpr ImVec4 TEXT       = FromHex(0xE7E9EA);
    inline constexpr ImVec4 TEXT_MUTED = FromHex(0x969DA2);

    // Silver as a fill behind text: dim enough that TEXT stays readable on it.
    inline constexpr ImVec4 ACCENT_FILL        = WithAlpha(ACCENT, 0.25f);
    inline constexpr ImVec4 ACCENT_FILL_STRONG = WithAlpha(ACCENT, 0.35f);

    // Sets every ImGui colour from the palette.
    void Apply(ImGuiStyle& style);
}
