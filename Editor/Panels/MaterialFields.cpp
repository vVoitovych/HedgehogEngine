#include "MaterialFields.hpp"

#include "Panels/AssetDragDrop.hpp"
#include "Panels/MaterialSlots.hpp"
#include "Widgets/PropertyFields.hpp"

#include "DialogueWindows/api/TextureDialogue.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <string>

namespace Editor
{
    namespace
    {
        using HedgehogEngine::MaterialData;
        using HedgehogEngine::MaterialTextureSlot;

        // A drop-down of the known textures for one slot, taking a Content panel texture too, then
        // Load... and Clear on a row of their own. Returns the slot's new map, or nothing when unchanged.
        std::optional<std::string> DrawMapRow(const MaterialSlotInfo& slot, const std::string& current,
                                              const HedgehogEngine::TextureContainer& textures,
                                              const FS::FileSystemManager& fileSystem)
        {
            std::optional<std::string> chosen;
            PropertyLabel(slot.Label);
            const bool open = ImGui::BeginCombo(slot.Id, current.empty() ? "None" : current.c_str());
            if (const auto drop = AcceptAssetDrop({ ContentType::Texture }))
            {
                if (auto path = ToMaterialTexturePath(drop->VirtualPath))
                    chosen = std::move(path);
                else
                    LOGWARNING("A material names textures under assets:// only; '", drop->VirtualPath, "' is not.");
            }
            if (open)
            {
                for (const std::string& texture : textures.GetTexturePathes())
                {
                    const bool selected = texture == current;
                    if (ImGui::Selectable(texture.c_str(), selected))
                        chosen = texture;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            PropertyLabel("");
            ImGui::PushID(slot.Id);
            if (ImGui::Button("Load..."))
            {
                if (const char* file = DialogueWindows::TextureOpenDialogue())
                {
                    const auto virtualPath = fileSystem.ToVirtualPath(file);
                    if (auto path = virtualPath ? ToMaterialTexturePath(*virtualPath) : std::nullopt)
                        chosen = std::move(path);
                    else
                        LOGERROR("Texture path is not under assets://: ", file);
                }
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(current.empty());
            if (ImGui::Button("Clear"))
                chosen = std::string();
            ImGui::EndDisabled();
            ImGui::PopID();

            if (chosen && *chosen == current)
                chosen.reset();
            return chosen;
        }
    }

    void DrawMaterialFields(HedgehogEngine::MaterialContainer& materials, const HedgehogEngine::TextureContainer& textures,
                            size_t index, const FS::FileSystemManager& fileSystem)
    {
        MaterialData& material = materials.GetMaterialDataByIndex(index);
        bool          edited   = false;

        const char* typeNames[] = { "Opaque", "Cutoff", "Transparent" };
        int         type        = static_cast<int>(material.type);
        PropertyLabel("Type");
        if (ImGui::Combo("##Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
        {
            material.type = static_cast<HedgehogEngine::MaterialType>(type);
            edited        = true;
        }
        if (material.type == HedgehogEngine::MaterialType::Transparent)
        {
            PropertyLabel("Transparency");
            edited |= ImGui::SliderFloat("##Transparency", &material.transparency, 0.0f, 1.0f);
        }

        // Factors are linear, as glTF stores them; the emissive colour may pass 1.
        PropertyLabel("Base colour");
        edited |= ImGui::ColorEdit4("##BaseColorFactor", material.baseColorFactor.GetBuffer(), ImGuiColorEditFlags_Float);
        PropertyLabel("Metallic");
        edited |= ImGui::SliderFloat("##Metallic", &material.metallic, 0.0f, 1.0f);
        PropertyLabel("Roughness");
        edited |= ImGui::SliderFloat("##Roughness", &material.roughness, 0.0f, 1.0f);
        PropertyLabel("Normal scale");
        edited |= ImGui::DragFloat("##NormalScale", &material.normalScale, 0.01f, 0.0f, 4.0f);
        PropertyLabel("Occlusion strength");
        edited |= ImGui::SliderFloat("##OcclusionStrength", &material.occlusionStrength, 0.0f, 1.0f);
        PropertyLabel("Emissive");
        edited |= ImGui::ColorEdit3("##EmissiveFactor", material.emissiveFactor.GetBuffer(),
                                    ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        if (edited)
            materials.SetMaterialDirty(index);

        for (const MaterialSlotInfo& slot : GetMaterialSlots())
        {
            const std::string current = HedgehogEngine::GetMaterialTexture(material, slot.Slot);
            if (const auto chosen = DrawMapRow(slot, current, textures, fileSystem))
                materials.SetTexture(index, slot.Slot, *chosen);
        }
    }
}
