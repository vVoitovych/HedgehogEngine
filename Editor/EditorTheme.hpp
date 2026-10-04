#pragma once

#include "imgui.h"

#include <cstdint>

// The editor's charcoal and blue-grey palette, with its style metrics and UI font size. Every UI colour comes from here; colours that carry
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
    inline constexpr ImVec4 ACCENT     = FromHex(0x7A9CC6); // selected / active; use sparingly
    inline constexpr ImVec4 TEXT       = FromHex(0xE7E9EA);
    inline constexpr ImVec4 TEXT_MUTED = FromHex(0x969DA2);

    // The accent as a fill behind text: dim enough that TEXT stays readable on it.
    inline constexpr ImVec4 ACCENT_FILL        = WithAlpha(ACCENT, 0.25f);
    inline constexpr ImVec4 ACCENT_FILL_STRONG = WithAlpha(ACCENT, 0.35f);

    // Selected rows, headers and cells: opaque blue-greys, so overlapping fills do not add up.
    inline constexpr ImVec4 SELECTION        = FromHex(0x34404D);
    inline constexpr ImVec4 SELECTION_STRONG = FromHex(0x41505F);

    // The play-mode toolbar button that matches the current state.
    inline constexpr ImVec4 PLAY_TINT = FromHex(0x4C8DDB);

    // A prefab instance's root in the hierarchy: its name and icon.
    inline constexpr ImVec4 PREFAB_TINT = FromHex(0x78B4F0);

    // The UI font's size in pixels, before any global scale.
    inline constexpr float FONT_SIZE      = 15.0f;
    // The Console's monospace font is ImGui's bitmap font, crisp only at its own size.
    inline constexpr float MONO_FONT_SIZE = 13.0f;

    // Sets every ImGui colour from the palette, and the style's rounding, padding and spacing.
    void Apply(ImGuiStyle& style);

    // The palette is written in sRGB. An sRGB render target encodes what ImGui writes once more, so
    // for one every style colour is converted to linear first; alpha is left as it is.
    void ConvertToLinear(ImGuiStyle& style);

    // A palette colour as ImGui must be handed it outside the style (a draw-list colour, an image
    // tint): converted to linear once ConvertToLinear has run, as written otherwise.
    [[nodiscard]] ImVec4 Resolve(const ImVec4& color);
}
