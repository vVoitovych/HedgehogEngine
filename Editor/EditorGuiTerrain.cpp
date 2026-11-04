#include "EditorGui.hpp"

#include "Panels/AssetDragDrop.hpp"
#include "Panels/MaterialSlots.hpp"
#include "Tools/Terrain/TerrainEditing.hpp"
#include "Widgets/PropertyFields.hpp"

#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "DialogueWindows/api/TerrainDialogue.hpp"
#include "DialogueWindows/api/TextureDialogue.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Editor
{
    namespace
    {
        constexpr std::string_view ASSETS_PREFIX  = "assets://";
        // Where New terrain... suggests saving, under assets://.
        constexpr const char*      TERRAIN_FOLDER = "Terrain";

        // A layer's map: its path, taking a Content panel texture dropped on it, then Load... and
        // Clear. Returns the new path (empty to clear), or nothing when unchanged.
        std::optional<std::string> DrawLayerMapRow(const char* label, const char* id, const std::string& current,
                                                   const FS::FileSystemManager& fileSystem)
        {
            std::optional<std::string> chosen;
            ImGui::PushID(id);
            PropertyLabel(label);
            ImGui::Button(current.empty() ? "None" : current.c_str(), ImVec2(-1.0f, 0.0f));
            ImGui::SetItemTooltip("Drop a texture here from the Project panel.");
            if (const auto drop = AcceptAssetDrop({ ContentType::Texture }))
            {
                if (auto path = ToMaterialTexturePath(drop->VirtualPath))
                    chosen = std::move(path);
                else
                    LOGWARNING("A terrain names textures under assets:// only; '", drop->VirtualPath, "' is not.");
            }
            PropertyLabel("");
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

    void EditorGui::DrawTerrainComponent(HedgehogEngine::Engine& context)
    {
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const ECS::Entity entity        = m_SelectedEntity.value();
        const auto*       info          = engineContext.GetComponentTypes().Find("TerrainComponent");
        // The header, its Enabled checkbox and the reflected rows, Terrain taking a dropped .terrain.
        if (!info || !DrawRegisteredComponent(context, *info) || !ecs.HasComponent<HedgehogEngine::TerrainComponent>(entity))
            return;

        auto& terrain = ecs.GetComponent<HedgehogEngine::TerrainComponent>(entity);
        if (!BeginPropertyTable("##TerrainFile"))
            return;
        PropertyLabel("");
        if (ImGui::Button("New terrain..."))
            CreateTerrain(engineContext, entity);
        if (terrain.TerrainIndex < 0)
        {
            PropertyLabel("");
            ImGui::TextDisabled(terrain.Terrain.empty() ? "No terrain file." : "The terrain file does not load.");
            EndPropertyTable();
            return;
        }
        EndPropertyTable();
        DrawTerrainData(engineContext, static_cast<size_t>(terrain.TerrainIndex));
    }

    void EditorGui::DrawTerrainData(HedgehogEngine::EngineContext& engineContext, size_t index)
    {
        HedgehogEngine::TerrainContainer& terrains = engineContext.GetResourceCatalog().GetTerrainContainer();
        HedgehogEngine::TerrainData&      data     = terrains.GetTerrain(index);
        const auto                        edited   = [&](HedgehogEngine::TerrainDirty parts)
        {
            terrains.MarkDirty(index, parts);
            m_EditedTerrains.insert(terrains.GetPath(index));
        };

        ImGui::PushID(terrains.GetPath(index).c_str());
        if (BeginPropertyTable("##TerrainTile"))
        {
            PropertyLabel("Size");
            float size = data.Size;
            if (ImGui::DragFloat("##Size", &size, 0.5f, 1.0f, HedgehogEngine::MAX_TERRAIN_SIZE, "%.1f m") && size >= 1.0f)
            {
                data.Size = size;
                edited(HedgehogEngine::TerrainDirty::Heights);
            }

            PropertyLabel("Resolution");
            if (ImGui::BeginCombo("##Resolution", std::to_string(data.Resolution).c_str()))
            {
                for (const uint32_t resolution : TERRAIN_RESOLUTIONS)
                {
                    const bool selected = resolution == data.Resolution;
                    if (ImGui::Selectable(std::to_string(resolution).c_str(), selected) && !selected &&
                        ResampleTerrain(data, resolution))
                        edited(HedgehogEngine::TerrainDirty::Heights);
                }
                ImGui::EndCombo();
            }
            ImGui::SetItemTooltip("Samples on a side; changing it resamples the heights.");

            PropertyLabel("Height scale");
            float heightScale = data.HeightScale;
            if (ImGui::DragFloat("##HeightScale", &heightScale, 0.5f, 0.1f, 10000.0f, "%.1f m") &&
                SetTerrainHeightScale(data, heightScale))
                edited(HedgehogEngine::TerrainDirty::Heights);
            ImGui::SetItemTooltip("The highest a height may be; heights above it are clamped.");
            EndPropertyTable();
        }

        const FS::FileSystemManager& fileSystem = engineContext.GetFileSystem();
        for (uint32_t layer = 0; layer < HedgehogEngine::TERRAIN_LAYER_COUNT; ++layer)
        {
            HedgehogEngine::TerrainLayer& values = data.Layers[layer];
            ImGui::PushID(static_cast<int>(layer));
            const std::string title = "Layer " + std::to_string(layer) +
                                      (values.BaseColor.empty() ? std::string() : " (" + std::filesystem::path(values.BaseColor).stem().string() + ")");
            if (ImGui::TreeNodeEx("##Layer", layer == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s", title.c_str()))
            {
                if (BeginPropertyTable("##LayerFields"))
                {
                    if (auto map = DrawLayerMapRow("Base colour", "BaseColor", values.BaseColor, fileSystem))
                    {
                        values.BaseColor = *map;
                        edited(HedgehogEngine::TerrainDirty::Layers);
                    }
                    if (auto map = DrawLayerMapRow("Normal map", "NormalMap", values.NormalMap, fileSystem))
                    {
                        values.NormalMap = *map;
                        edited(HedgehogEngine::TerrainDirty::Layers);
                    }
                    PropertyLabel("Tiling");
                    if (ImGui::DragFloat("##Tiling", &values.Tiling, 0.1f, 0.01f, 1000.0f, "%.2f"))
                        edited(HedgehogEngine::TerrainDirty::Layers);
                    ImGui::SetItemTooltip("How many times the layer's maps repeat across the tile.");
                    PropertyLabel("Metallic");
                    if (ImGui::SliderFloat("##Metallic", &values.Metallic, 0.0f, 1.0f))
                        edited(HedgehogEngine::TerrainDirty::Layers);
                    PropertyLabel("Roughness");
                    if (ImGui::SliderFloat("##Roughness", &values.Roughness, 0.0f, 1.0f))
                        edited(HedgehogEngine::TerrainDirty::Layers);
                    EndPropertyTable();
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        const bool unsaved = m_EditedTerrains.contains(terrains.GetPath(index));
        if (ImGui::Button("Save terrain") && terrains.Save(index))
            m_EditedTerrains.erase(terrains.GetPath(index));
        if (unsaved)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("Unsaved changes");
        }
        ImGui::PopID();
    }

    void EditorGui::CreateTerrain(HedgehogEngine::EngineContext& engineContext, ECS::Entity entity)
    {
        auto&      fileSystem = engineContext.GetFileSystem();
        const auto folder     = fileSystem.ResolvePhysical(std::string(ASSETS_PREFIX) + TERRAIN_FOLDER);
        if (!folder)
            return;
        std::error_code error;
        std::filesystem::create_directories(*folder, error);
        const std::string& name      = engineContext.GetECS().GetComponent<ECS::HierarchyComponent>(entity).Name;
        const std::string  suggested = (std::filesystem::path(*folder) / (name + ".terrain")).make_preferred().string();

        const char* picked = DialogueWindows::TerrainSaveDialogue(suggested.c_str());
        if (!picked)
            return;
        std::filesystem::path chosen(picked);
        if (chosen.extension() != HedgehogEngine::TERRAIN_EXTENSION)
            chosen += HedgehogEngine::TERRAIN_EXTENSION;
        const auto virtualPath = fileSystem.ToVirtualPath(chosen.string());
        if (!virtualPath || !virtualPath->starts_with(ASSETS_PREFIX))
        {
            LOGERROR("Editor: '", chosen.string(), "' is not under assets://, so it cannot be a terrain.");
            return;
        }
        if (!fileSystem.WriteTextFile(*virtualPath, HedgehogEngine::WriteTerrain(HedgehogEngine::MakeDefaultTerrain())))
        {
            LOGERROR("Editor: ", *virtualPath, " cannot be written.");
            return;
        }
        LOGINFO("Editor: created ", *virtualPath, ".");
        engineContext.GetECS().GetComponent<HedgehogEngine::TerrainComponent>(entity).Terrain =
            virtualPath->substr(ASSETS_PREFIX.size());
    }
}
