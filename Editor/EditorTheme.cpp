#include "EditorTheme.hpp"

#include <cmath>

namespace Editor::Theme
{
    namespace
    {
        [[nodiscard]] float SrgbToLinear(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        [[nodiscard]] ImVec4 ToLinear(const ImVec4& color)
        {
            return { SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), color.w };
        }

        // Whether ConvertToLinear has run: the render target encodes to sRGB.
        bool s_LinearOutput = false;
    }

    void Apply(ImGuiStyle& style)
    {
        constexpr ImVec4 CLEAR = { 0.0f, 0.0f, 0.0f, 0.0f };

        ImVec4* c = style.Colors;

        c[ImGuiCol_Text]                       = TEXT;
        c[ImGuiCol_TextDisabled]               = TEXT_MUTED;
        c[ImGuiCol_WindowBg]                   = PANEL;
        c[ImGuiCol_ChildBg]                    = PANEL;
        c[ImGuiCol_PopupBg]                    = PANEL;
        c[ImGuiCol_Border]                     = FRAME;
        c[ImGuiCol_BorderShadow]               = CLEAR;

        c[ImGuiCol_FrameBg]                    = FRAME;
        c[ImGuiCol_FrameBgHovered]             = HOVER;
        c[ImGuiCol_FrameBgActive]              = HOVER;

        c[ImGuiCol_TitleBg]                    = MAIN_BG;
        c[ImGuiCol_TitleBgActive]              = FRAME;
        c[ImGuiCol_TitleBgCollapsed]           = MAIN_BG;
        c[ImGuiCol_MenuBarBg]                  = MAIN_BG;

        c[ImGuiCol_ScrollbarBg]                = CLEAR;
        c[ImGuiCol_ScrollbarGrab]              = FRAME;
        c[ImGuiCol_ScrollbarGrabHovered]       = HOVER;
        c[ImGuiCol_ScrollbarGrabActive]        = TEXT_MUTED;

        c[ImGuiCol_CheckMark]                  = ACCENT;
        c[ImGuiCol_CheckboxSelectedBg]         = FRAME;
        c[ImGuiCol_SliderGrab]                 = ACCENT;
        c[ImGuiCol_SliderGrabActive]           = TEXT;

        c[ImGuiCol_Button]                     = FRAME;
        c[ImGuiCol_ButtonHovered]              = HOVER;
        c[ImGuiCol_ButtonActive]               = ACCENT_FILL_STRONG;

        c[ImGuiCol_Header]                     = SELECTION;
        c[ImGuiCol_HeaderHovered]              = HOVER;
        c[ImGuiCol_HeaderActive]               = SELECTION_STRONG;

        c[ImGuiCol_Separator]                  = FRAME;
        c[ImGuiCol_SeparatorHovered]           = HOVER;
        c[ImGuiCol_SeparatorActive]            = ACCENT;

        c[ImGuiCol_ResizeGrip]                 = WithAlpha(HOVER, 0.5f);
        c[ImGuiCol_ResizeGripHovered]          = HOVER;
        c[ImGuiCol_ResizeGripActive]           = ACCENT;

        c[ImGuiCol_InputTextCursor]            = TEXT;

        // A selected tab takes the panel's colour and an accent overline; the rest sit darker.
        c[ImGuiCol_Tab]                        = MAIN_BG;
        c[ImGuiCol_TabHovered]                 = HOVER;
        c[ImGuiCol_TabSelected]                = PANEL;
        c[ImGuiCol_TabSelectedOverline]        = ACCENT;
        c[ImGuiCol_TabDimmed]                  = MAIN_BG;
        c[ImGuiCol_TabDimmedSelected]          = PANEL;
        c[ImGuiCol_TabDimmedSelectedOverline]  = HOVER;

        c[ImGuiCol_PlotLines]                  = TEXT_MUTED;
        c[ImGuiCol_PlotLinesHovered]           = ACCENT;
        c[ImGuiCol_PlotHistogram]              = TEXT_MUTED;
        c[ImGuiCol_PlotHistogramHovered]       = ACCENT;

        c[ImGuiCol_TableHeaderBg]              = FRAME;
        c[ImGuiCol_TableBorderStrong]          = HOVER;
        c[ImGuiCol_TableBorderLight]           = FRAME;
        c[ImGuiCol_TableRowBg]                 = CLEAR;
        c[ImGuiCol_TableRowBgAlt]              = WithAlpha(TEXT, 0.03f);

        c[ImGuiCol_TextLink]                   = ACCENT;
        c[ImGuiCol_TextSelectedBg]             = ACCENT_FILL_STRONG;
        c[ImGuiCol_TreeLines]                  = HOVER;
        c[ImGuiCol_DragDropTarget]             = ACCENT;
        c[ImGuiCol_DragDropTargetBg]           = WithAlpha(ACCENT, 0.10f);
        c[ImGuiCol_UnsavedMarker]              = TEXT;

        c[ImGuiCol_NavCursor]                  = ACCENT;
        c[ImGuiCol_NavWindowingHighlight]      = WithAlpha(TEXT, 0.7f);
        c[ImGuiCol_NavWindowingDimBg]          = WithAlpha(MAIN_BG, 0.6f);
        c[ImGuiCol_ModalWindowDimBg]           = WithAlpha(MAIN_BG, 0.6f);

        // Flat panels, softly rounded controls.
        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 0.0f;
        style.FrameRounding     = 3.0f;
        style.PopupRounding     = 4.0f;
        style.TabRounding       = 3.0f;
        style.GrabRounding      = 3.0f;
        style.ScrollbarRounding = 6.0f;
        style.ScrollbarSize     = 10.0f;
        style.FramePadding      = { 6.0f, 4.0f };
        style.ItemSpacing       = { 6.0f, 4.0f };
        style.IndentSpacing     = 14.0f;
        style.WindowBorderSize  = 0.0f;
        style.FrameBorderSize   = 0.0f;
        style.TabBarBorderSize  = 1.0f;
        style.FontSizeBase      = FONT_SIZE;
    }

    void ConvertToLinear(ImGuiStyle& style)
    {
        for (ImVec4& color : style.Colors)
            color = ToLinear(color);
        s_LinearOutput = true;
    }

    ImVec4 Resolve(const ImVec4& color)
    {
        return s_LinearOutput ? ToLinear(color) : color;
    }
}
