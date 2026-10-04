#pragma once

#include "EcsSerialization/api/Reflection/PropertyDescriptor.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include "EditorTheme.hpp"
#include "Widgets/PropertyFields.hpp"

#include "imgui.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Reflection
{
    // The selected prefab instance entity's overridden properties, which the inspector sets while it
    // draws (ActivePrefabOverrideMarks), so reflected rows show theirs in the prefab colour and
    // offer Revert and Apply to Prefab on a right-click of the value; the pick waits in Requested.
    struct PrefabOverrideMarks
    {
        struct Request
        {
            std::string Component;
            std::string Property;
            bool        Apply = false; // else revert
        };

        std::vector<std::pair<std::string, std::string>> Overridden; // (component key, property)
        std::optional<Request>                           Requested;

        [[nodiscard]] bool IsOverridden(std::string_view component, std::string_view property) const
        {
            return std::any_of(Overridden.begin(), Overridden.end(),
                               [&](const auto& entry) { return entry.first == component && entry.second == property; });
        }
    };

    inline PrefabOverrideMarks*& ActivePrefabOverrideMarks()
    {
        static PrefabOverrideMarks* marks = nullptr;
        return marks;
    }

    // Draws prop as a property row (Widgets/PropertyFields): its name on the left, its widget
    // filling the right. A guiOverride draws its own row, or nothing. componentKey (the component's
    // serializer key) lets a prefab instance's overridden row be marked.
    inline bool RenderPropertyWidget(void* comp, const PropertyDescriptor& prop, const char* componentKey = nullptr)
    {
        if (prop.type == TypeTag::Raw)                  return false;
        if (HasFlag(prop.flags, PropertyFlags::Hidden)) return false;

        if (prop.guiOverride)
            return prop.guiOverride(comp, prop);

        PrefabOverrideMarks* marks      = componentKey ? ActivePrefabOverrideMarks() : nullptr;
        const bool           overridden = marks && marks->IsOverridden(componentKey, prop.name);
        if (overridden)
            ImGui::PushStyleColor(ImGuiCol_Text, Editor::Theme::Resolve(Editor::Theme::PREFAB_TINT));
        Editor::PropertyLabel(prop.name);
        if (overridden)
            ImGui::PopStyleColor();
        ImGui::PushID(prop.name);

        bool changed = false;
        switch (prop.type)
        {
        case TypeTag::Float:
            if (HasFlag(prop.flags, PropertyFlags::IsSlider))
                changed = ImGui::SliderFloat("##v", FieldPtr<float>(comp, prop), prop.sliderMin, prop.sliderMax);
            else
                changed = ImGui::DragFloat("##v", FieldPtr<float>(comp, prop));
            break;
        case TypeTag::Double:
            { float tmp = static_cast<float>(*FieldPtr<double>(comp, prop));
              if (ImGui::DragFloat("##v", &tmp)) { *FieldPtr<double>(comp, prop) = static_cast<double>(tmp); changed = true; } }
            break;
        case TypeTag::UInt:
            { int tmp = static_cast<int>(*FieldPtr<uint32_t>(comp, prop));
              if (ImGui::DragInt("##v", &tmp, 1.0f, 0, INT_MAX)) { *FieldPtr<uint32_t>(comp, prop) = static_cast<uint32_t>(tmp); changed = true; } }
            break;
        case TypeTag::Int:
            if (HasFlag(prop.flags, PropertyFlags::IsSlider))
                changed = ImGui::SliderInt("##v", FieldPtr<int32_t>(comp, prop),
                                           static_cast<int>(prop.sliderMin), static_cast<int>(prop.sliderMax));
            else
                changed = ImGui::DragInt("##v", FieldPtr<int32_t>(comp, prop));
            break;
        case TypeTag::Bool:
            changed = ImGui::Checkbox("##v", FieldPtr<bool>(comp, prop));
            break;
        case TypeTag::String:
            { auto* str = FieldPtr<std::string>(comp, prop);
              char buf[256]; buf[str->copy(buf, sizeof(buf)-1)] = '\0';
              if (ImGui::InputText("##v", buf, sizeof(buf))) { *str = buf; changed = true; } }
            break;
        case TypeTag::Vec2:
            changed = ImGui::DragFloat2("##v", FieldPtr<HM::Vector2>(comp, prop)->GetBuffer());
            break;
        case TypeTag::Vec3:
            if (HasFlag(prop.flags, PropertyFlags::IsColor))
                changed = ImGui::ColorEdit3("##v", FieldPtr<HM::Vector3>(comp, prop)->GetBuffer(), ImGuiColorEditFlags_NoLabel);
            else
                changed = Editor::DragVector3Xyz("##v", FieldPtr<HM::Vector3>(comp, prop)->GetBuffer());
            break;
        case TypeTag::Vec4:
            if (HasFlag(prop.flags, PropertyFlags::IsColor))
                changed = ImGui::ColorEdit4("##v", FieldPtr<HM::Vector4>(comp, prop)->GetBuffer(), ImGuiColorEditFlags_NoLabel);
            else
                changed = ImGui::DragFloat4("##v", FieldPtr<HM::Vector4>(comp, prop)->GetBuffer());
            break;
        case TypeTag::Enum:
            if (prop.enumLabels && prop.enumLabelCount > 0)
            { int idx = 0; std::memcpy(&idx, prop.accessor(comp), sizeof(int32_t));
              if (ImGui::Combo("##v", &idx, prop.enumLabels, prop.enumLabelCount))
              { std::memcpy(prop.accessor(comp), &idx, sizeof(int32_t)); changed = true; } }
            else
              changed = ImGui::DragInt("##v", reinterpret_cast<int*>(prop.accessor(comp)));
            break;
        case TypeTag::Entity:
            if (const ECS::Entity entity = *FieldPtr<ECS::Entity>(comp, prop); entity == ECS::INVALID_ENTITY)
                ImGui::TextDisabled("None");
            else
                ImGui::Text("Entity %zu", entity);
            break;
        default:
            ImGui::TextDisabled("[unsupported]");
            break;
        }

        if (overridden)
        {
            ImGui::SetItemTooltip("Overridden by this prefab instance. Right-click to revert or apply it.");
            if (ImGui::BeginPopupContextItem("##override"))
            {
                if (ImGui::MenuItem("Revert"))
                    marks->Requested = PrefabOverrideMarks::Request{ componentKey, prop.name, false };
                if (ImGui::MenuItem("Apply to Prefab"))
                    marks->Requested = PrefabOverrideMarks::Request{ componentKey, prop.name, true };
                ImGui::EndPopup();
            }
        }
        ImGui::PopID();
        return changed;
    }

    // Every property of a component, in one table of property rows.
    inline bool RenderComponentGui(void* comp, std::span<const PropertyDescriptor> props, const char* componentKey = nullptr)
    {
        if (!Editor::BeginPropertyTable("##properties"))
            return false;
        bool anyChanged = false;
        for (const auto& prop : props)
            anyChanged |= RenderPropertyWidget(comp, prop, componentKey);
        Editor::EndPropertyTable();
        return anyChanged;
    }
}
