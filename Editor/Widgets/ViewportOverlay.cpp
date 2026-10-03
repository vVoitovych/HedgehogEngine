#include "ViewportOverlay.hpp"

#include "AxisGizmo.hpp"
#include "EditorTheme.hpp"

#include <algorithm>
#include <cstdio>

namespace Editor
{
    namespace
    {
        constexpr float GIZMO_RADIUS = 28.0f; // pixels from the centre to a full-length axis
        constexpr float GIZMO_MARGIN = 14.0f; // from the image's edges
        constexpr float AXIS_DOT     = 7.0f;  // the labelled end of an axis

        constexpr const char* AXIS_LABELS[] = { "X", "Y", "Z" };

        // X red, Y green, Z blue, as the reference draws them.
        constexpr ImVec4 AXIS_COLORS[] = {
            Theme::FromHex(0xE5484D), Theme::FromHex(0x6CC24A), Theme::FromHex(0x4C8DDB)
        };
    }

    void DrawAxisGizmo(ImDrawList& drawList, ImVec2 imageMin, ImVec2 imageSize, const HM::Matrix4x4& view)
    {
        const float  labelHeight = ImGui::GetTextLineHeight();
        const ImVec2 center      = { imageMin.x + imageSize.x - GIZMO_MARGIN - GIZMO_RADIUS - AXIS_DOT,
                                     imageMin.y + GIZMO_MARGIN + GIZMO_RADIUS + AXIS_DOT };
        if (imageSize.x < 4.0f * GIZMO_RADIUS || imageSize.y < 4.0f * GIZMO_RADIUS + labelHeight)
            return;

        // Back to front, so the axis pointing at the viewer is drawn over the others.
        const auto axes  = ProjectWorldAxes(view);
        size_t     order[] = { 0, 1, 2 };
        std::sort(std::begin(order), std::end(order),
                  [&axes](size_t a, size_t b) { return axes[a].Depth < axes[b].Depth; });

        for (const size_t axis : order)
        {
            const ImVec2 tip   = { center.x + axes[axis].X * GIZMO_RADIUS, center.y + axes[axis].Y * GIZMO_RADIUS };
            const ImU32  color = ImGui::ColorConvertFloat4ToU32(Theme::Resolve(AXIS_COLORS[axis]));
            drawList.AddLine(center, tip, color, 2.0f);
            drawList.AddCircleFilled(tip, AXIS_DOT, color);

            const ImVec2 labelSize = ImGui::CalcTextSize(AXIS_LABELS[axis]);
            drawList.AddText(ImVec2(tip.x - labelSize.x * 0.5f, tip.y - labelSize.y * 0.5f),
                             ImGui::ColorConvertFloat4ToU32(Theme::Resolve(Theme::MAIN_BG)), AXIS_LABELS[axis]);
        }

        constexpr const char* PROJECTION = "Persp";
        const ImVec2 textSize = ImGui::CalcTextSize(PROJECTION);
        drawList.AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + GIZMO_RADIUS + AXIS_DOT + 4.0f),
                         ImGui::ColorConvertFloat4ToU32(Theme::Resolve(Theme::TEXT)), PROJECTION);
    }

    void DrawPassCount(ImDrawList& drawList, ImVec2 imageMin, ImVec2 imageSize, size_t passCount)
    {
        char text[48];
        std::snprintf(text, sizeof(text), "%zu render graph passes", passCount);
        const ImVec2 at = { imageMin.x + 8.0f, imageMin.y + imageSize.y - ImGui::GetTextLineHeight() - 8.0f };
        // A dark shadow keeps it readable over a bright scene.
        drawList.AddText(ImVec2(at.x + 1.0f, at.y + 1.0f), ImGui::ColorConvertFloat4ToU32(Theme::Resolve(Theme::MAIN_BG)), text);
        drawList.AddText(at, ImGui::ColorConvertFloat4ToU32(Theme::Resolve(Theme::TEXT_MUTED)), text);
    }
}
