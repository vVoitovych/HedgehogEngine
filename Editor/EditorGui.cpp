#include "EditorGui.hpp"
#include "Panels/ConsolePanel.hpp"

#include "HedgehogEngine/HedgehogEngine/Context/HedgehogEngine.hpp"
#include "HedgehogEngine/HedgehogEngine/Context/EngineContext.hpp"
#include "HedgehogEngine/HedgehogEngine/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogEngine/HedgehogEngine/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogEngine/HedgehogEngine/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogEngine/HedgehogEngine/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "Components/api/GameObjectHelpers.hpp"
#include "Components/api/MeshSystem.hpp"
#include "Components/api/RenderSystem.hpp"
#include "Components/api/LightSystem.hpp"
#include "Components/api/ScriptSystem.hpp"
#include "Components/api/LightComponent.hpp"
#include "Components/api/MeshComponent.hpp"
#include "Components/api/RenderComponent.hpp"
#include "Components/api/ScriptComponent.hpp"

#include "ContentLoader/api/CommonFunctions.hpp"
#include "DialogueWindows/api/MaterialDialogue.hpp"
#include "DialogueWindows/api/SceneDialogue.hpp"

#include "imgui.h"

#include <algorithm>
#include <stdexcept>

using namespace Scene;

namespace Editor
{
    EditorGui::EditorGui()
        : m_ConsolePanel(std::make_unique<ConsolePanel>())
    {
        if (m_Settings.Load(k_SettingsPath) && m_Settings.dockLayout.IsValid())
            m_DockSystem.GetLayout() = m_Settings.dockLayout;
    }

    EditorGui::~EditorGui()
    {
        m_Settings.dockLayout = m_DockSystem.GetLayout();
        m_Settings.Save(k_SettingsPath);
    }

    // ─── Top-level entry ─────────────────────────────────────────────────────

    void EditorGui::Draw(HedgehogEngine::HedgehogEngine& context, void* sceneViewTextureId)
    {
        m_SceneViewHovered = false;

        ImVec4* styleColors = ImGui::GetStyle().Colors;
        const ImVec4 panelBg(m_Settings.panelBgColor[0], m_Settings.panelBgColor[1],
                             m_Settings.panelBgColor[2], 1.0f);
        styleColors[ImGuiCol_WindowBg]  = panelBg;
        styleColors[ImGuiCol_ChildBg]   = panelBg;
        styleColors[ImGuiCol_PopupBg]   = panelBg;
        styleColors[ImGuiCol_MenuBarBg] = panelBg;

        m_SceneViewTextureId = sceneViewTextureId;

        DrawMainMenu(context);

        const float menuH = ImGui::GetFrameHeight();

        m_DockSystem.Draw(
            [this]() { DrawToolbarContent(); },
            [this, &context](PanelId panel) { DrawPanelContent(panel, context, m_SceneViewTextureId); },
            menuH);

        DrawSettingsWindow(context);
    }

    // ─── Panel dispatch ───────────────────────────────────────────────────────

    void EditorGui::DrawPanelContent(PanelId panel, HedgehogEngine::HedgehogEngine& context, void* sceneViewTextureId)
    {
        switch (panel)
        {
        case PanelId::SceneHierarchy: DrawSceneHierarchy(context);          break;
        case PanelId::Inspector:      DrawInspector(context);                break;
        case PanelId::Console:        m_ConsolePanel->Draw();                break;
        default:                      DrawSceneViewContent(sceneViewTextureId); break;
        }
    }

    void EditorGui::DrawSceneViewContent(void* sceneViewTextureId)
    {
        if (!ImGui::BeginTabBar("##SceneGameTabs"))
            return;

        if (ImGui::BeginTabItem("Scene"))
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            m_SceneViewWidth  = static_cast<uint32_t>(std::max(1.0f, avail.x));
            m_SceneViewHeight = static_cast<uint32_t>(std::max(1.0f, avail.y));

            if (sceneViewTextureId)
            {
                ImGui::Image(sceneViewTextureId, avail);
                m_SceneViewHovered = ImGui::IsItemHovered();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Game"))
        {
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ─── Main menu ───────────────────────────────────────────────────────────

    void EditorGui::DrawMainMenu(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto* renderSystem  = engineContext.GetRenderSystem();
        ECS::Entity root    = engineContext.GetRootEntity();

        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {
                engineContext.ResetScene();
                m_SelectedEntity.reset();
            }
            if (ImGui::MenuItem("Rename"))
            {
                char* newName = DialogueWindows::SceneRenameDialogue();
                if (newName != nullptr)
                    engineContext.SetSceneName(newName);
            }
            if (ImGui::MenuItem("Open"))
            {
                char* path = DialogueWindows::SceneOpenDialogue();
                if (path != nullptr)
                {
                    engineContext.LoadScene(path);
                    m_SelectedEntity.reset();
                }
            }
            if (ImGui::MenuItem("Save"))
            {
                char* path = DialogueWindows::SceneSaveDialogue();
                if (path != nullptr)
                    engineContext.SaveScene(path);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Create game object"))
                Components::CreateGameObject(ecs, root, m_SelectedEntity);

            ImGui::Separator();
            if (ImGui::BeginMenu("Add component"))
            {
                if (ImGui::MenuItem("Mesh component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<MeshComponent>(e))
                    {
                        ecs.AddComponent(e, MeshComponent{ Scene::MeshSystem::sDefaultMeshPath });
                        meshSystem->Update(ecs, e);
                    }
                }
                if (ImGui::MenuItem("Render component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<RenderComponent>(e))
                    {
                        ecs.AddComponent(e, RenderComponent{});
                        renderSystem->Update(ecs, e);
                    }
                }
                if (ImGui::MenuItem("Light component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<LightComponent>(e))
                        ecs.AddComponent(e, LightComponent{});
                }
                if (ImGui::MenuItem("Script component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<ScriptComponent>(e))
                        ecs.AddComponent(e, ScriptComponent{});
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Create material"))
                engineContext.GetMaterialContainer().CreateNewMaterial();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows"))
        {
            auto& layout = m_DockSystem.GetLayout();
            for (int i = 0; i < PANEL_ID_COUNT; ++i)
            {
                const PanelId pid     = static_cast<PanelId>(i);
                const bool    visible = layout.m_PanelVisible[i];
                if (ImGui::MenuItem(PanelName(pid), nullptr, visible))
                {
                    if (visible)
                        layout.HidePanel(pid);
                    else
                        layout.ShowPanel(pid);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Settings")) { m_SettingsWindowOpen = true; }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // ─── Toolbar ─────────────────────────────────────────────────────────────

    void EditorGui::DrawToolbarContent()
    {
        const bool isEdit  = (m_EditorMode == EditorMode::Edit);
        const bool isPlay  = (m_EditorMode == EditorMode::Play);
        const bool isPause = (m_EditorMode == EditorMode::Pause);

        if (isPlay)  ImGui::BeginDisabled();
        if (ImGui::Button("  Play  "))
            m_EditorMode = EditorMode::Play;
        if (isPlay)  ImGui::EndDisabled();

        ImGui::SameLine();

        if (isEdit)  ImGui::BeginDisabled();
        if (ImGui::Button(isPause ? " Resume " : "  Pause  "))
            m_EditorMode = isPause ? EditorMode::Play : EditorMode::Pause;
        if (isEdit)  ImGui::EndDisabled();

        ImGui::SameLine();

        if (isEdit)  ImGui::BeginDisabled();
        if (ImGui::Button("  Stop  "))
            m_EditorMode = EditorMode::Edit;
        if (isEdit)  ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();

        const char* modeText = [this]() -> const char*
        {
            switch (m_EditorMode)
            {
            case EditorMode::Play:  return "PLAY";
            case EditorMode::Pause: return "PAUSE";
            default:                return "EDIT";
            }
        }();
        ImGui::Text("Mode: %s", modeText);
    }

    // ─── Scene hierarchy ─────────────────────────────────────────────────────

    void EditorGui::DrawSceneHierarchy(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        ECS::Entity root    = engineContext.GetRootEntity();

        ImGui::SeparatorText(engineContext.GetSceneName().c_str());

        const ImVec2 fullWidth = ImVec2(-FLT_MIN, 0.0f);
        if (ImGui::Button("Create object", fullWidth))
            Components::CreateGameObject(ecs, root, m_SelectedEntity);

        if (ImGui::Button("Delete object", fullWidth))
        {
            if (m_SelectedEntity.has_value() && m_SelectedEntity.value() != root)
            {
                Components::DeleteGameObject(ecs, m_SelectedEntity.value());
                m_SelectedEntity.reset();
            }
        }

        ImGui::Separator();

        int index = 0;
        DrawHierarchyNode(context, root, index);
    }

    void EditorGui::DrawHierarchyNode(HedgehogEngine::HedgehogEngine& context, ECS::Entity entity, int& index)
    {
        auto& ecs       = context.GetEngineContext().GetECS();
        auto& component = ecs.GetComponent<HierarchyComponent>(entity);

        ImGuiTreeNodeFlags nodeFlags =
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_DefaultOpen;

        const bool isSelected =
            m_SelectedEntity.has_value() && (entity == m_SelectedEntity.value());
        if (isSelected)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;

        auto toggleSelection = [&]()
        {
            if (m_SelectedEntity.has_value() && m_SelectedEntity.value() == entity)
                m_SelectedEntity.reset();
            else
                m_SelectedEntity = entity;
        };

        if (!component.m_Children.empty())
        {
            const bool nodeOpen = ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());

            if (ImGui::IsItemClicked())
                toggleSelection();
            ++index;

            if (nodeOpen)
            {
                for (auto child : component.m_Children)
                    DrawHierarchyNode(context, child, index);
                ImGui::TreePop();
            }
        }
        else
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(
                reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                toggleSelection();
            ++index;
        }
    }

    // ─── Inspector ───────────────────────────────────────────────────────────

    void EditorGui::DrawInspector(HedgehogEngine::HedgehogEngine& context)
    {
        if (m_SelectedEntity.has_value())
        {
            DrawEntityTitle(context);
            DrawTransformComponent(context);
            DrawLightComponent(context);
            DrawMeshComponent(context);
            DrawRenderComponent(context);
            DrawScriptComponent(context);
        }
        else
        {
            ImGui::TextDisabled("No entity selected.");
        }
    }

    void EditorGui::DrawEntityTitle(HedgehogEngine::HedgehogEngine& context)
    {
        auto& ecs       = context.GetEngineContext().GetECS();
        auto  entity    = m_SelectedEntity.value();
        auto& hierarchy = ecs.GetComponent<HierarchyComponent>(entity);
        auto  name      = hierarchy.m_Name;

        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::InputText("##name", &name[0], name.capacity() + 1))
                hierarchy.m_Name = name;
        }
    }

    void EditorGui::DrawTransformComponent(HedgehogEngine::HedgehogEngine& context)
    {
        auto& ecs    = context.GetEngineContext().GetECS();
        auto  entity = m_SelectedEntity.value();

        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& transform = ecs.GetComponent<TransformComponent>(entity);

        ImGui::SeparatorText("Position");
        ImGui::DragFloat("pos x", &transform.m_Position.x(), 0.5f);
        ImGui::DragFloat("pos y", &transform.m_Position.y(), 0.5f);
        ImGui::DragFloat("pos z", &transform.m_Position.z(), 0.5f);

        ImGui::SeparatorText("Rotation");
        ImGui::DragFloat("rot x", &transform.m_Rotation.x(), 0.5f);
        ImGui::DragFloat("rot y", &transform.m_Rotation.y(), 0.5f);
        ImGui::DragFloat("rot z", &transform.m_Rotation.z(), 0.5f);

        ImGui::SeparatorText("Scale");
        ImGui::DragFloat("scale x", &transform.m_Scale.x(), 0.5f);
        ImGui::DragFloat("scale y", &transform.m_Scale.y(), 0.5f);
        ImGui::DragFloat("scale z", &transform.m_Scale.z(), 0.5f);
    }

    void EditorGui::DrawMeshComponent(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<MeshComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto&    mesh          = ecs.GetComponent<MeshComponent>(entity);
        const auto& meshPaths  = meshSystem->GetMeshes();
        uint64_t selectedIndex = mesh.m_MeshIndex.value_or(0);

        if (ImGui::BeginCombo("mesh", mesh.m_MeshPath.c_str()))
        {
            for (uint64_t i = 0; i < meshPaths.size(); ++i)
            {
                const bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(meshPaths[i].c_str(), isSelected))
                {
                    mesh.m_MeshPath = meshPaths[i];
                    meshSystem->Update(ecs, entity);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Load mesh"))
            meshSystem->LoadMesh(ecs, entity);
        if (ImGui::Button("Remove mesh"))
        {
            if (ecs.HasComponent<MeshComponent>(entity))
                ecs.RemoveComponent<MeshComponent>(entity);
        }
    }

    void EditorGui::DrawRenderComponent(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* renderSystem  = engineContext.GetRenderSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<RenderComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& render        = ecs.GetComponent<RenderComponent>(entity);
        const auto& materials = renderSystem->GetMaterials();

        bool visible = render.m_IsVisible;
        if (ImGui::Checkbox("Visible", &visible))
            render.m_IsVisible = visible;

        if (!materials.empty())
        {
            const uint64_t selectedIndex = render.m_MaterialIndex.value_or(0);
            if (ImGui::BeginCombo("material", render.m_Material.c_str()))
            {
                for (uint64_t i = 0; i < materials.size(); ++i)
                {
                    const bool isSelected = (selectedIndex == i);
                    if (ImGui::Selectable(materials[i].c_str(), isSelected))
                    {
                        render.m_Material = materials[i];
                        renderSystem->Update(ecs, entity);
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Load material"))
        {
            char* path = DialogueWindows::MaterialOpenDialogue();
            if (path != nullptr)
            {
                render.m_Material = ContentLoader::GetAssetRelativetlyPath(path);
                renderSystem->Update(ecs, entity);
            }
        }

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            if (ImGui::Button("Save material"))
                engineContext.GetMaterialContainer().SaveMaterial(render.m_MaterialIndex.value());
        }

        if (ImGui::Button("Remove render"))
        {
            if (ecs.HasComponent<RenderComponent>(entity))
                ecs.RemoveComponent<RenderComponent>(entity);
        }

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            ImGui::SeparatorText("Material");
            auto& materialContainer = engineContext.GetMaterialContainer();
            auto& textureContainer  = engineContext.GetTextureContainer();
            auto& materialData      = materialContainer.GetMaterialDataByIndex(
                render.m_MaterialIndex.value());

            const char* typeNames[] = { "Opaque", "Cutoff", "Transparent" };
            int materialType = static_cast<int>(materialData.type);
            if (ImGui::Combo("Type", &materialType, typeNames, IM_ARRAYSIZE(typeNames)))
                materialData.type = static_cast<HedgehogEngine::MaterialType>(materialType);

            const auto& texturePaths = textureContainer.GetTexturePathes();
            int selectedTexture      = static_cast<int>(
                textureContainer.GetTextureIndex(materialData.baseColor));

            if (ImGui::BeginCombo("baseColor", materialData.baseColor.c_str()))
            {
                for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
                {
                    const bool isSelected = (selectedTexture == i);
                    if (ImGui::Selectable(texturePaths[i].c_str(), isSelected))
                    {
                        materialData.baseColor = texturePaths[i];
                        materialContainer.SetMaterialDirty(render.m_MaterialIndex.value());
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Load texture"))
                materialContainer.LoadBaseTexture(render.m_MaterialIndex.value());

            if (materialData.type == HedgehogEngine::MaterialType::Transparent)
            {
                float transparency = materialData.transparency;
                if (ImGui::SliderFloat("Transparency", &transparency, 0.0f, 1.0f))
                {
                    if (materialData.transparency != transparency)
                    {
                        materialData.transparency = transparency;
                        materialContainer.SetMaterialDirty(render.m_MaterialIndex.value());
                    }
                }
            }
        }
    }

    namespace
    {
        Scene::LightType IndexToLightType(int index)
        {
            switch (index)
            {
            case 0: return Scene::LightType::DirectionLight;
            case 1: return Scene::LightType::PointLight;
            case 2: return Scene::LightType::SpotLight;
            default: throw std::runtime_error("Invalid light type index");
            }
        }

        int LightTypeToIndex(Scene::LightType type)
        {
            switch (type)
            {
            case Scene::LightType::DirectionLight: return 0;
            case Scene::LightType::PointLight:     return 1;
            case Scene::LightType::SpotLight:      return 2;
            default: throw std::runtime_error("Invalid light type");
            }
        }
    }

    void EditorGui::DrawLightComponent(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* lightSystem   = engineContext.GetLightSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<LightComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& light = ecs.GetComponent<LightComponent>(entity);

        bool enabled = light.m_Enable;
        if (ImGui::Checkbox("Enabled", &enabled))
            light.m_Enable = enabled;

        const char* typeNames[] = { "Direction Light", "Point Light", "Spot Light" };
        int lightTypeIndex = LightTypeToIndex(light.m_LightType);
        if (ImGui::Combo("Type", &lightTypeIndex, typeNames, IM_ARRAYSIZE(typeNames)))
            light.m_LightType = IndexToLightType(lightTypeIndex);

        float color[3] = { light.m_Color.r(), light.m_Color.g(), light.m_Color.b() };
        if (ImGui::ColorEdit3("Color", color))
            light.m_Color = { color[0], color[1], color[2] };

        float intensity = light.m_Intensity;
        if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 3.0f))
            light.m_Intensity = intensity;

        if (lightTypeIndex > 0)
        {
            float radius = light.m_Radius;
            if (ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f))
                light.m_Radius = radius;

            if (lightTypeIndex > 1)
            {
                float coneAngle = light.m_ConeAngle;
                if (ImGui::SliderFloat("Cone angle", &coneAngle, 0.1f, 179.9f))
                    light.m_ConeAngle = coneAngle;
            }
        }

        if (lightTypeIndex == 0)
        {
            bool castShadows = light.m_CastShadows;
            if (ImGui::Checkbox("Cast shadows", &castShadows))
                lightSystem->SetShadowCasting(ecs, entity, castShadows);
        }

        if (ImGui::Button("Remove light"))
        {
            if (ecs.HasComponent<LightComponent>(entity))
                ecs.RemoveComponent<LightComponent>(entity);
        }
    }

    void EditorGui::DrawScriptComponent(HedgehogEngine::HedgehogEngine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* scriptSystem  = engineContext.GetScriptSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<ScriptComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& component = ecs.GetComponent<ScriptComponent>(entity);

        bool enabled = component.m_Enable;
        if (ImGui::Checkbox("Enabled", &enabled))
            component.m_NewEnable = enabled;

        std::string scriptName = component.m_ScriptPath.empty()
            ? "No script selected" : component.m_ScriptPath;
        ImGui::InputText("Script", &scriptName[0], scriptName.capacity() + 1);

        if (ImGui::Button("Load script"))
            scriptSystem->ChangeScript(entity, ecs);

        if (!component.m_Params.empty())
            ImGui::SeparatorText("Parameters");

        for (auto& [name, param] : component.m_Params)
        {
            switch (param.type)
            {
            case Scene::ParamType::Boolean:
            {
                bool bVal = std::get<bool>(param.value);
                if (ImGui::Checkbox(name.c_str(), &bVal))
                {
                    param.value = bVal;
                    param.dirty = true;
                }
                break;
            }
            case Scene::ParamType::Number:
            {
                float nVal = std::get<float>(param.value);
                if (ImGui::DragFloat(name.c_str(), &nVal, 0.05f))
                {
                    param.value = nVal;
                    param.dirty = true;
                }
                break;
            }
            default:
                break;
            }
        }

        if (ImGui::Button("Remove script"))
        {
            if (ecs.HasComponent<ScriptComponent>(entity))
                ecs.RemoveComponent<ScriptComponent>(entity);
        }
    }

    // ─── Settings window ─────────────────────────────────────────────────────

    void EditorGui::DrawSettingsWindow(HedgehogEngine::HedgehogEngine& context)
    {
        if (!m_SettingsWindowOpen)
            return;

        ImGui::SetNextWindowSize(ImVec2(600.0f, 400.0f), ImGuiCond_Appearing);
        ImGui::Begin("Settings", &m_SettingsWindowOpen, ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Main"))
            {
                if (ImGui::MenuItem("Close")) { m_SettingsWindowOpen = false; }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (ImGui::CollapsingHeader("Editor"))
        {
            ImGui::ColorEdit3("Panel background", m_Settings.panelBgColor);

            ImGui::Spacing();
            if (ImGui::Button("Save settings"))
                m_Settings.Save(k_SettingsPath);
            ImGui::SameLine();
            if (ImGui::Button("Load settings"))
                m_Settings.Load(k_SettingsPath);
        }

        auto& engineContext = context.GetEngineContext();
        auto& settings      = engineContext.GetSettings();

        if (ImGui::CollapsingHeader("Shadows"))
        {
            auto& shadow = settings.GetShadowmapSettings();

            float split1 = shadow->GetSplit1();
            float split2 = shadow->GetSplit2();
            float split3 = shadow->GetSplit3();

            int mapSize = shadow->GetShadowmapSize();
            if (ImGui::InputInt("Map size", &mapSize))
            {
                shadow->SetShadowmapSize(mapSize);
                mapSize = shadow->GetShadowmapSize();
            }

            float lambda = shadow->GetCascadeSplitLambda();
            if (ImGui::SliderFloat("Lambda", &lambda, 0.0f, 1.0f))
                shadow->SetCascadeSplitLambda(lambda);

            int cascades = shadow->GetCascadesCount();
            if (ImGui::SliderInt("Cascades", &cascades, 1, 4))
            {
                shadow->SetCascadesCount(cascades);
                shadow->SetDefaultSplits();
                split1 = shadow->GetSplit1();
                split2 = shadow->GetSplit2();
                split3 = shadow->GetSplit3();
            }

            if (cascades > 1)
                if (ImGui::SliderFloat("Split 1", &split1, 0.0f, split2))
                    shadow->SetSplit1(split1);
            if (cascades > 2)
                if (ImGui::SliderFloat("Split 2", &split2, split1, split3))
                    shadow->SetSplit2(split2);
            if (cascades > 3)
                if (ImGui::SliderFloat("Split 3", &split3, split2, 100.0f))
                    shadow->SetSplit3(split3);

            ImGui::SeparatorText("Debug");
            const auto& shadowDir = engineContext.GetLightSystem()->GetShadowDir();
            if (shadowDir.has_value())
            {
                float x = shadowDir.value().x();
                float y = shadowDir.value().y();
                float z = shadowDir.value().z();
                ImGui::SeparatorText("Shadow direction");
                ImGui::DragFloat("dir x", &x, 0.5f);
                ImGui::DragFloat("dir y", &y, 0.5f);
                ImGui::DragFloat("dir z", &z, 0.5f);
            }
        }

        ImGui::End();
    }
}
