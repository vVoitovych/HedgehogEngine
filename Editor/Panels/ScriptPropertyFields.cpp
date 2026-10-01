#include "ScriptPropertyFields.hpp"

#include "AssetDragDrop.hpp"
#include "ContentTypes.hpp"
#include "EntityDragDrop.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    namespace
    {
        using HedgehogEngine::ScriptProperty;
        using HedgehogEngine::ScriptPropertyType;
        using HedgehogScripting::ScriptPropertyDeclaration;

        constexpr std::string_view ASSETS_PREFIX = "assets://";
        constexpr size_t           TEXT_CAPACITY = 256;

        // Whether value holds the variant alternative type uses, so a field never reads the wrong one.
        bool Holds(const ScriptProperty& property)
        {
            switch (property.Type)
            {
            case ScriptPropertyType::Number:    return std::holds_alternative<float>(property.Value);
            case ScriptPropertyType::Bool:      return std::holds_alternative<bool>(property.Value);
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:     return std::holds_alternative<HM::Vector3>(property.Value);
            case ScriptPropertyType::String:
            case ScriptPropertyType::AssetRef:  return std::holds_alternative<std::string>(property.Value);
            case ScriptPropertyType::EntityRef: return std::holds_alternative<ECS::Entity>(property.Value);
            }
            return false;
        }

        // A read-only text box showing text, as the entity and asset fields draw their value.
        void ReadOnlyText(const char* label, const std::string& text)
        {
            char buffer[TEXT_CAPACITY];
            strncpy_s(buffer, text.c_str(), sizeof(buffer) - 1);
            ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_ReadOnly);
        }

        std::string EntityLabel(const ECS::ECS& ecs, ECS::Entity entity)
        {
            if (entity == ECS::INVALID_ENTITY)
                return "None";
            if (entity >= ECS::MAX_ENTITIES || !ecs.IsAlive(entity) || !ecs.HasComponent<ECS::HierarchyComponent>(entity))
                return "Missing (entity " + std::to_string(entity) + ")";
            return ecs.GetComponent<ECS::HierarchyComponent>(entity).Name;
        }

        // The asset types an AssetRef takes: the one its declaration names, or any file when it
        // names none the Content panel knows.
        std::vector<ContentType> AcceptedAssetTypes(const std::string& assetType)
        {
            if (const auto type = FindContentTypeByName(assetType))
                return { *type };
            std::vector<ContentType> any;
            for (size_t index = 0; index < CONTENT_TYPE_COUNT; ++index)
                if (static_cast<ContentType>(index) != ContentType::Folder)
                    any.push_back(static_cast<ContentType>(index));
            return any;
        }

        // The widget for property's type; returns true when it changed property.Value.
        bool DrawValueWidget(ScriptProperty& property, const ScriptPropertyDeclaration* declaration, const ECS::ECS& ecs)
        {
            const char* label = property.Name.c_str();
            switch (property.Type)
            {
            case ScriptPropertyType::Number:
            {
                const bool  ranged = declaration && (declaration->Min || declaration->Max);
                const float min    = declaration && declaration->Min ? *declaration->Min : -FLT_MAX;
                const float max    = declaration && declaration->Max ? *declaration->Max : FLT_MAX;
                float       value  = std::get<float>(property.Value);
                if (!ImGui::DragFloat(label, &value, 0.05f, min, max, "%.3f", ranged ? ImGuiSliderFlags_AlwaysClamp : 0))
                    return false;
                property.Value = value;
                return true;
            }
            case ScriptPropertyType::Bool:
            {
                bool value = std::get<bool>(property.Value);
                if (!ImGui::Checkbox(label, &value))
                    return false;
                property.Value = value;
                return true;
            }
            case ScriptPropertyType::Vector3:
            case ScriptPropertyType::Color:
            {
                HM::Vector3& vector = std::get<HM::Vector3>(property.Value);
                float        value[3] = { vector.x(), vector.y(), vector.z() };
                const bool   changed  = property.Type == ScriptPropertyType::Color ? ImGui::ColorEdit3(label, value)
                                                                                   : ImGui::DragFloat3(label, value, 0.05f);
                if (!changed)
                    return false;
                property.Value = HM::Vector3(value[0], value[1], value[2]);
                return true;
            }
            case ScriptPropertyType::String:
            {
                char buffer[TEXT_CAPACITY];
                strncpy_s(buffer, std::get<std::string>(property.Value).c_str(), sizeof(buffer) - 1);
                if (!ImGui::InputText(label, buffer, sizeof(buffer)))
                    return false;
                property.Value = std::string(buffer);
                return true;
            }
            case ScriptPropertyType::EntityRef:
            {
                ReadOnlyText(label, EntityLabel(ecs, std::get<ECS::Entity>(property.Value)));
                const auto dropped = AcceptEntityDrop();
                if (!dropped)
                    return false;
                property.Value = *dropped;
                return true;
            }
            case ScriptPropertyType::AssetRef:
            {
                const std::string& path = std::get<std::string>(property.Value);
                ReadOnlyText(label, path.empty() ? "None" : path);
                const std::vector<ContentType> accepted = AcceptedAssetTypes(property.AssetType);
                const auto                     dropped  = AcceptAssetDrop(std::span<const ContentType>(accepted));
                if (!dropped)
                    return false;
                // Kept under assets://, as the script and the scene file name it.
                std::string virtualPath = dropped->VirtualPath;
                if (virtualPath.starts_with(ASSETS_PREFIX))
                    virtualPath.erase(0, ASSETS_PREFIX.size());
                property.Value = virtualPath;
                return true;
            }
            }
            return false;
        }

        // One property's row: its widget, the declaration's tooltip and, when it differs from the
        // declared default, a Reset button. Returns true when it changed property.
        bool DrawField(ScriptProperty& property, const ScriptPropertyDeclaration* declaration, const ECS::ECS& ecs)
        {
            ImGui::PushID(property.Name.c_str());
            bool changed = DrawValueWidget(property, declaration, ecs);
            if (declaration && !declaration->Tooltip.empty())
                ImGui::SetItemTooltip("%s", declaration->Tooltip.c_str());

            if (declaration && property.Value != declaration->Default.Value)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset"))
                {
                    property.Value = declaration->Default.Value;
                    changed        = true;
                }
                ImGui::SetItemTooltip("Back to the script's default");
            }
            ImGui::PopID();
            return changed;
        }
    }

    bool DrawScriptProperties(HedgehogEngine::ScriptComponent&                       component,
                              const std::vector<ScriptPropertyDeclaration>*          declarations,
                              const ECS::ECS&                                        ecs)
    {
        const bool any = declarations ? !declarations->empty() : !component.Properties.empty();
        if (!any)
            return false;
        ImGui::SeparatorText("Properties");

        bool changed = false;
        if (!declarations)
        {
            for (ScriptProperty& property : component.Properties)
                if (Holds(property))
                    changed |= DrawField(property, nullptr, ecs);
            return changed;
        }

        for (const ScriptPropertyDeclaration& declaration : *declarations)
        {
            // The saved value when it fits the declaration, else the default; the component only
            // gains the property when it is edited.
            const ScriptProperty* saved = HedgehogEngine::FindScriptProperty(component, declaration.Default.Name);
            ScriptProperty        value = saved && saved->Type == declaration.Default.Type && Holds(*saved)
                                              ? *saved
                                              : declaration.Default;
            value.AssetType = declaration.Default.AssetType;
            if (DrawField(value, &declaration, ecs))
            {
                HedgehogEngine::SetScriptProperty(component, value);
                changed = true;
            }
        }

        for (const ScriptProperty& saved : component.Properties)
        {
            const bool declared = std::ranges::any_of(*declarations, [&](const ScriptPropertyDeclaration& declaration)
                                                      { return declaration.Default.Name == saved.Name; });
            if (!declared)
                ImGui::TextDisabled("%s: saved, but the script no longer declares it", saved.Name.c_str());
        }
        return changed;
    }
}
