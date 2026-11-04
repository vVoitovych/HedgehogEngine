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
#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "HedgehogExtract/api/CameraMath.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"

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
        // The brush's ring: its segments, its colour (RGBA8, red lowest) and how far it floats over
        // the surface, so the depth test does not hide it.
        constexpr uint32_t         BRUSH_RING_SEGMENTS = 48;
        constexpr uint32_t         BRUSH_RING_COLOR    = 0xff40c8ffu; // amber
        constexpr float            BRUSH_RING_LIFT     = 0.05f;

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

        DrawTerrainBrushFields();

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

    void EditorGui::DrawTerrainBrushFields()
    {
        if (!BeginPropertyTable("##TerrainBrush"))
            return;
        PropertyLabel("Sculpt");
        ImGui::Checkbox("##Sculpt", &m_TerrainSculpt);
        ImGui::SetItemTooltip("Left drag in the Scene view sculpts the terrain; right drag still pans the camera.");
        ImGui::BeginDisabled(!m_TerrainSculpt);
        PropertyLabel("Tool");
        int tool = static_cast<int>(m_TerrainBrush.Tool);
        if (ImGui::Combo("##Tool", &tool, TERRAIN_BRUSH_TOOL_NAMES, IM_ARRAYSIZE(TERRAIN_BRUSH_TOOL_NAMES)))
            m_TerrainBrush.Tool = static_cast<TerrainBrushTool>(tool);
        PropertyLabel("Radius");
        ImGui::DragFloat("##Radius", &m_TerrainBrush.Radius, 0.1f, 0.5f, 200.0f, "%.1f m");
        PropertyLabel("Strength");
        ImGui::DragFloat("##Strength", &m_TerrainBrush.Strength, 0.05f, 0.05f, 100.0f, "%.2f");
        PropertyLabel("Falloff");
        ImGui::SliderFloat("##Falloff", &m_TerrainBrush.Falloff, 0.0f, 1.0f);
        ImGui::EndDisabled();
        EndPropertyTable();
    }

    bool EditorGui::IsTerrainSculpting(HedgehogEngine::EngineContext& engineContext) const
    {
        if (!m_TerrainSculpt || !m_SelectedEntity || engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit)
            return false;
        auto& ecs = engineContext.GetECS();
        return ecs.IsAlive(*m_SelectedEntity) && ecs.HasComponent<HedgehogEngine::TerrainComponent>(*m_SelectedEntity) &&
               ecs.GetComponent<HedgehogEngine::TerrainComponent>(*m_SelectedEntity).TerrainIndex >= 0;
    }

    void EditorGui::UpdateTerrainBrush(HedgehogEngine::EngineContext& engineContext, const HX::RenderCamera& sceneCamera,
                                       HX::RenderScene& scene)
    {
        if (!m_TerrainBrushPointer || !IsTerrainSculpting(engineContext))
            return;
        auto&                    ecs       = engineContext.GetECS();
        const ECS::Entity        entity    = *m_SelectedEntity;
        const auto&              component = ecs.GetComponent<HedgehogEngine::TerrainComponent>(entity);
        const HM::Matrix4x4&     world     = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity).ObjMatrix;
        auto&                    terrains  = engineContext.GetResourceCatalog().GetTerrainContainer();
        const size_t             index     = static_cast<size_t>(component.TerrainIndex);
        HedgehogEngine::TerrainData& data  = terrains.GetTerrain(index);

        // The pointer's ray, into the tile's local space.
        const float aspect = static_cast<float>(m_SceneViewWidth) / static_cast<float>(std::max(1u, m_SceneViewHeight));
        const HX::Ray     ray    = HX::MakePickRay(sceneCamera, aspect, m_TerrainBrushPointer->U, m_TerrainBrushPointer->V);
        bool              inverted = false;
        const HM::Matrix4x4 toLocal = world.Inverse(inverted);
        if (!inverted)
            return;
        const HM::Vector4 origin    = toLocal * HM::Vector4(ray.Origin.x(), ray.Origin.y(), ray.Origin.z(), 1.0f);
        const HM::Vector4 direction = toLocal * HM::Vector4(ray.Direction.x(), ray.Direction.y(), ray.Direction.z(), 0.0f);
        const std::optional<HedgehogEngine::TerrainRayHit> hit = HedgehogEngine::RaycastTerrain(
            data, HM::Vector3(origin.x(), origin.y(), origin.z()), HM::Vector3(direction.x(), direction.y(), direction.z()),
            1.0e6f);
        if (!hit)
            return;

        // A frame of the stroke; Flatten holds the height where the stroke began.
        if (m_TerrainStrokeBegan)
            m_TerrainBrush.FlattenHeight = hit->Point.z();
        if (m_TerrainStroke &&
            ApplyTerrainBrush(data, m_TerrainBrush, hit->Point.x(), hit->Point.y(), ImGui::GetIO().DeltaTime))
        {
            terrains.MarkDirty(index, HedgehogEngine::TerrainDirty::Heights);
            m_EditedTerrains.insert(terrains.GetPath(index));
        }

        // The ring, on the surface as it is now, in world space.
        const std::vector<HM::Vector3> ring =
            MakeBrushRing(data, hit->Point.x(), hit->Point.y(), m_TerrainBrush.Radius, BRUSH_RING_SEGMENTS);
        const auto toWorld = [&](const HM::Vector3& point)
        {
            const HM::Vector4 p = world * HM::Vector4(point.x(), point.y(), point.z() + BRUSH_RING_LIFT, 1.0f);
            return HM::Vector3(p.x(), p.y(), p.z());
        };
        for (size_t i = 0; i < ring.size(); ++i)
        {
            scene.DebugLines.push_back({ toWorld(ring[i]), BRUSH_RING_COLOR });
            scene.DebugLines.push_back({ toWorld(ring[(i + 1) % ring.size()]), BRUSH_RING_COLOR });
        }
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
