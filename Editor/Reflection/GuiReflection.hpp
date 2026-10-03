#pragma once

#include "EcsSerialization/api/Reflection/PropertyDescriptor.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include "Widgets/PropertyFields.hpp"

#include "imgui.h"

#include <climits>
#include <cstring>
#include <span>
#include <string>

namespace Reflection
{
    // Draws prop as a property row (Widgets/PropertyFields): its name on the left, its widget
    // filling the right. A guiOverride draws its own row, or nothing.
    inline bool RenderPropertyWidget(void* comp, const PropertyDescriptor& prop)
    {
        if (prop.type == TypeTag::Raw)                  return false;
        if (HasFlag(prop.flags, PropertyFlags::Hidden)) return false;

        if (prop.guiOverride)
            return prop.guiOverride(comp, prop);

        Editor::PropertyLabel(prop.name);
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
        default:
            ImGui::TextDisabled("[unsupported]");
            break;
        }

        ImGui::PopID();
        return changed;
    }

    // Every property of a component, in one table of property rows.
    inline bool RenderComponentGui(void* comp, std::span<const PropertyDescriptor> props)
    {
        if (!Editor::BeginPropertyTable("##properties"))
            return false;
        bool anyChanged = false;
        for (const auto& prop : props)
            anyChanged |= RenderPropertyWidget(comp, prop);
        Editor::EndPropertyTable();
        return anyChanged;
    }
}
