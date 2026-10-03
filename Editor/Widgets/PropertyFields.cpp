#include "PropertyFields.hpp"

#include "IconWidgets.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>

namespace Editor
{
    namespace
    {
        constexpr float LABEL_COLUMN_WEIGHT  = 0.4f;
        constexpr float WIDGET_COLUMN_WEIGHT = 0.6f;

        constexpr const char* AXIS_LABELS[] = { "X", "Y", "Z" };
    }

    bool BeginPropertyTable(const char* id)
    {
        constexpr ImGuiTableFlags FLAGS = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX;
        if (!ImGui::BeginTable(id, 2, FLAGS))
            return false;
        ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, LABEL_COLUMN_WEIGHT);
        ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, WIDGET_COLUMN_WEIGHT);
        return true;
    }

    void EndPropertyTable()
    {
        ImGui::EndTable();
    }

    void PropertyLabel(const char* label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    bool DragVector3Xyz(const char* id, float* values, float speed)
    {
        const ImGuiStyle& style   = ImGui::GetStyle();
        const float       spacing = style.ItemInnerSpacing.x;
        const float       cell    = (ImGui::GetContentRegionAvail().x - 2.0f * spacing) / 3.0f;

        bool changed = false;
        ImGui::PushID(id);
        for (int axis = 0; axis < 3; ++axis)
        {
            if (axis > 0)
                ImGui::SameLine(0.0f, spacing);
            ImGui::PushID(axis);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", AXIS_LABELS[axis]);
            ImGui::SameLine(0.0f, spacing);
            ImGui::SetNextItemWidth(std::max(cell - ImGui::GetItemRectSize().x - spacing, 1.0f));
            changed |= ImGui::DragFloat("##value", &values[axis], speed, 0.0f, 0.0f, "%.2f");
            ImGui::PopID();
        }
        ImGui::PopID();
        return changed;
    }

    ComponentHeaderResult ComponentHeader(const char* label, const ComponentHeaderOptions& options)
    {
        ComponentHeaderResult result;
        const ImGuiStyle&     style = ImGui::GetStyle();

        // The label leaves room for the checkbox and the icon, drawn over the header afterwards.
        const float checkboxWidth = options.Enabled ? ImGui::GetFrameHeight() + style.ItemInnerSpacing.x : 0.0f;
        const float iconWidth     = ICON_SIZE_SMALL + style.ItemInnerSpacing.x;
        const float labelIndent   = checkboxWidth + iconWidth;

        constexpr ImGuiTreeNodeFlags FLAGS = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen |
                                             ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_AllowOverlap |
                                             ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        ImGui::PushID(label);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const int padSpaces = static_cast<int>(std::ceil(labelIndent / ImGui::CalcTextSize(" ").x)) + 1;
        result.Open = ImGui::TreeNodeEx("##header", FLAGS, "%*s%s", padSpaces, "", label);
        const ImVec2 headerMin  = ImGui::GetItemRectMin();
        const ImVec2 headerMax  = ImGui::GetItemRectMax();
        const float  headerH    = headerMax.y - headerMin.y;
        const float  contentX   = start.x + ImGui::GetTreeNodeToLabelSpacing();
        const ImVec2 afterHeader = ImGui::GetCursorScreenPos();

        if (options.Enabled)
        {
            ImGui::SetCursorScreenPos(ImVec2(contentX, headerMin.y + (headerH - ImGui::GetFrameHeight()) * 0.5f));
            result.EnabledChanged = ImGui::Checkbox("##enabled", options.Enabled);
        }
        DrawIcon(*ImGui::GetWindowDrawList(), options.Icon,
                 ImVec2(contentX + checkboxWidth, headerMin.y + (headerH - ICON_SIZE_SMALL) * 0.5f),
                 ICON_SIZE_SMALL, ImGui::GetColorU32(ImGuiCol_Text));

        if (options.Removable)
        {
            const float buttonSize = ICON_SIZE_SMALL + 2.0f * style.FramePadding.y;
            ImGui::SetCursorScreenPos(ImVec2(headerMax.x - buttonSize - style.FramePadding.x,
                                             headerMin.y + (headerH - buttonSize) * 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.y, style.FramePadding.y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            if (IconButton("##menu", options.MenuIcon, ICON_SIZE_SMALL, style.Colors[ImGuiCol_Text]))
                ImGui::OpenPopup("##componentMenu");
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            if (ImGui::BeginPopup("##componentMenu"))
            {
                if (ImGui::MenuItem("Remove component"))
                    result.RemoveRequested = true;
                ImGui::EndPopup();
            }
        }

        // Back under the header, as if nothing had been drawn over it.
        ImGui::SetCursorScreenPos(afterHeader);
        ImGui::PopID();
        return result;
    }
}
