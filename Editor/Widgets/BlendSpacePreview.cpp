#include "BlendSpacePreview.hpp"

#include "EditorTheme.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace Editor
{
    void DrawBlendSpacePreview(const std::vector<HedgehogAnimation::BlendChild>& children,
                               std::optional<std::array<float, 2>> point, float size)
    {
        // The square holds every child (and the point) with a margin, never smaller than -1 to 1.
        float extent = 1.0f;
        for (const HedgehogAnimation::BlendChild& child : children)
            extent = std::max({ extent, std::abs(child.Position[0]), std::abs(child.Position[1]) });
        if (point)
            extent = std::max({ extent, std::abs((*point)[0]), std::abs((*point)[1]) });
        extent *= 1.15f;

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(size, size));
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const auto  map  = [&](float x, float y)
        {
            // y up, as a gamepad stick's forward.
            return ImVec2(origin.x + (x / extent * 0.5f + 0.5f) * size, origin.y + (0.5f - y / extent * 0.5f) * size);
        };

        draw->AddRectFilled(origin, ImVec2(origin.x + size, origin.y + size), ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
        const ImU32 axis = ImGui::GetColorU32(ImGuiCol_Border);
        draw->AddLine(map(-extent, 0.0f), map(extent, 0.0f), axis);
        draw->AddLine(map(0.0f, -extent), map(0.0f, extent), axis);

        const ImU32 dot = ImGui::GetColorU32(ImGuiCol_Text);
        for (const HedgehogAnimation::BlendChild& child : children)
        {
            const ImVec2 at = map(child.Position[0], child.Position[1]);
            draw->AddCircleFilled(at, 3.5f, dot);
            draw->AddText(ImVec2(at.x + 6.0f, at.y - 7.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), child.Clip.c_str());
        }
        if (point)
            draw->AddCircle(map((*point)[0], (*point)[1]), 6.0f, ImGui::GetColorU32(Theme::Resolve(Theme::PLAY_TINT)), 0,
                            2.0f);
    }
}
