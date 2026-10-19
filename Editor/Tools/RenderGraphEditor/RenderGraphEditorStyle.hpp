#pragma once

#include "EditorTheme.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

// The render graph editor's colours, shared by the canvas and the Details panel, and the
// node canvases' theme, which the animator controller editor shares.
namespace Editor
{
    inline constexpr ImVec4 PASS_COLOR     = { 1.00f, 1.00f, 1.00f, 1.0f };
    inline constexpr ImVec4 RESOURCE_COLOR = { 0.55f, 0.80f, 1.00f, 1.0f };
    inline constexpr ImVec4 IMPORT_COLOR   = { 0.80f, 0.65f, 1.00f, 1.0f };
    inline constexpr ImVec4 OUTPUT_COLOR   = { 0.55f, 0.95f, 0.55f, 1.0f };
    inline constexpr ImVec4 PROBLEM_COLOR  = { 0.95f, 0.35f, 0.35f, 1.0f };

    // The canvas in the editor's palette; the node-kind title colours above stay as they are.
    inline void ApplyCanvasTheme(ax::NodeEditor::Style& style)
    {
        ImVec4* c = style.Colors;
        c[ax::NodeEditor::StyleColor_Bg]                  = Theme::MAIN_BG;
        c[ax::NodeEditor::StyleColor_Grid]                = Theme::WithAlpha(Theme::HOVER, 0.5f);
        c[ax::NodeEditor::StyleColor_NodeBg]              = Theme::WithAlpha(Theme::PANEL, 0.95f);
        c[ax::NodeEditor::StyleColor_NodeBorder]          = Theme::HOVER;
        c[ax::NodeEditor::StyleColor_HovNodeBorder]       = Theme::TEXT_MUTED;
        c[ax::NodeEditor::StyleColor_SelNodeBorder]       = Theme::ACCENT;
        c[ax::NodeEditor::StyleColor_NodeSelRect]         = Theme::WithAlpha(Theme::ACCENT, 0.15f);
        c[ax::NodeEditor::StyleColor_NodeSelRectBorder]   = Theme::WithAlpha(Theme::ACCENT, 0.5f);
        c[ax::NodeEditor::StyleColor_HovLinkBorder]       = Theme::TEXT_MUTED;
        c[ax::NodeEditor::StyleColor_SelLinkBorder]       = Theme::ACCENT;
        c[ax::NodeEditor::StyleColor_HighlightLinkBorder] = Theme::ACCENT;
        c[ax::NodeEditor::StyleColor_LinkSelRect]         = Theme::WithAlpha(Theme::ACCENT, 0.15f);
        c[ax::NodeEditor::StyleColor_LinkSelRectBorder]   = Theme::WithAlpha(Theme::ACCENT, 0.5f);
        c[ax::NodeEditor::StyleColor_PinRect]             = Theme::ACCENT_FILL;
        c[ax::NodeEditor::StyleColor_PinRectBorder]       = Theme::WithAlpha(Theme::ACCENT, 0.5f);
        c[ax::NodeEditor::StyleColor_Flow]                = Theme::ACCENT;
        c[ax::NodeEditor::StyleColor_FlowMarker]          = Theme::ACCENT;
        c[ax::NodeEditor::StyleColor_GroupBg]             = Theme::WithAlpha(Theme::MAIN_BG, 0.6f);
        c[ax::NodeEditor::StyleColor_GroupBorder]         = Theme::HOVER;
    }
}
