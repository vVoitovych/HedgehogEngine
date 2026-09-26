#pragma once

#include "imgui.h"

// The render graph editor's colours, shared by the canvas and the Details panel.
namespace Editor
{
    inline constexpr ImVec4 PASS_COLOR     = { 1.00f, 1.00f, 1.00f, 1.0f };
    inline constexpr ImVec4 RESOURCE_COLOR = { 0.55f, 0.80f, 1.00f, 1.0f };
    inline constexpr ImVec4 IMPORT_COLOR   = { 0.80f, 0.65f, 1.00f, 1.0f };
    inline constexpr ImVec4 OUTPUT_COLOR   = { 0.55f, 0.95f, 0.55f, 1.0f };
    inline constexpr ImVec4 PROBLEM_COLOR  = { 0.95f, 0.35f, 0.35f, 1.0f };
}
