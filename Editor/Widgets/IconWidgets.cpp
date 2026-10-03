#include "IconWidgets.hpp"

namespace Editor
{
    void DrawIcon(ImDrawList& drawList, void* icon, ImVec2 min, float size, ImU32 tint)
    {
        if (icon)
            drawList.AddImage(icon, min, ImVec2(min.x + size, min.y + size), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
    }

    bool IconButton(const char* strId, void* icon, float size, const ImVec4& tint)
    {
        bool pressed = false;
        if (icon)
        {
            pressed = ImGui::ImageButton(strId, icon, ImVec2(size, size), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                         ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tint);
        }
        else
        {
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            ImGui::PushID(strId);
            pressed = ImGui::Button("##icon", ImVec2(size + 2.0f * padding.x, size + 2.0f * padding.y));
            ImGui::PopID();
        }
        return pressed;
    }
}
