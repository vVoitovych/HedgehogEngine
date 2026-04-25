#include "EditorGui.hpp"
#include "Panels/ConsolePanel.hpp"

#include "HedgehogEngine/HedgehogContext/Context/Context.hpp"
#include "HedgehogEngine/HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogEngine/HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"

#include "imgui.h"

#include <algorithm>
#include <stdexcept>

namespace Editor
{
    EditorGui::EditorGui()
        : m_ConsolePanel(std::make_unique<ConsolePanel>())
    {
    }

    EditorGui::~EditorGui() = default;

    // ─── Top-level entry ─────────────────────────────────────────────────────

    void EditorGui::Draw(Context::Context& context, void* sceneViewTextureId)
    {
        ImVec4* styleColors = ImGui::GetStyle().Colors;
        const ImVec4 panelBg(m_PanelBgColor[0], m_PanelBgColor[1], m_PanelBgColor[2], 1.0f);
        styleColors[ImGuiCol_WindowBg]  = panelBg;
        styleColors[ImGuiCol_ChildBg]   = panelBg;
        styleColors[ImGuiCol_PopupBg]   = panelBg;
        styleColors[ImGuiCol_MenuBarBg] = panelBg;

        const ImGuiIO& io    = ImGui::GetIO();
        const float    W     = io.DisplaySize.x;
        const float    H     = io.DisplaySize.y;
        const float    menuH = ImGui::GetFrameHeight();

        DrawMainMenu(context);
        DrawEditorLayout(context, W, H, menuH, sceneViewTextureId);
        DrawSettingsWindow(context);
    }

    // ─── Layout ──────────────────────────────────────────────────────────────

    void EditorGui::DrawEditorLayout(Context::Context& context, float W, float H, float menuH, void* sceneViewTextureId)
    {
        const float availH = H - menuH;

        // Clamp panel sizes every frame so window resizing stays coherent
        const float minCenterW = k_MinPanelSize;
        m_LeftPanelWidth  = std::clamp(m_LeftPanelWidth,  k_MinPanelSize,
            W - m_RightPanelWidth - minCenterW - 2.0f * k_SplitterThickness);
        m_RightPanelWidth = std::clamp(m_RightPanelWidth, k_MinPanelSize,
            W - m_LeftPanelWidth  - minCenterW - 2.0f * k_SplitterThickness);
        m_ConsolePanelHeight = std::clamp(m_ConsolePanelHeight, k_MinPanelSize,
            availH - k_ToolbarHeight - k_SplitterThickness - k_MinPanelSize);

        const float centerW = W - m_LeftPanelWidth - m_RightPanelWidth - 2.0f * k_SplitterThickness;

        // Transparent, decoration-free fullscreen container
        ImGui::SetNextWindowPos(ImVec2(0.0f, menuH), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(W, availH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);

        constexpr ImGuiWindowFlags k_LayoutFlags =
            ImGuiWindowFlags_NoDecoration          |
            ImGuiWindowFlags_NoMove                |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings       |
            ImGuiWindowFlags_NoScrollbar           |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("##editor_layout", nullptr, k_LayoutFlags);

        // ── Left panel ───────────────────────────────────────────────────────
        ImGui::BeginChild("##left_panel", ImVec2(m_LeftPanelWidth, availH), true);
        DrawSceneHierarchy(context);
        ImGui::EndChild();

        // ── Left vertical splitter ───────────────────────────────────────────
        ImGui::SameLine(0.0f, 0.0f);
        DrawLeftSplitter(availH, W);

        // ── Center column ────────────────────────────────────────────────────
        ImGui::SameLine(0.0f, 0.0f);
        DrawCenterColumn(context, centerW, availH, sceneViewTextureId);

        // ── Right vertical splitter ──────────────────────────────────────────
        ImGui::SameLine(0.0f, 0.0f);
        DrawRightSplitter(availH, W);

        // ── Right panel ──────────────────────────────────────────────────────
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##right_panel", ImVec2(-1.0f, availH), true);
        DrawInspector(context);
        ImGui::EndChild();

        ImGui::End();
    }

    void EditorGui::DrawLeftSplitter(float availH, float W)
    {
        ImGui::InvisibleButton("##left_vsplit", ImVec2(k_SplitterThickness, availH));
        if (ImGui::IsItemActive())
        {
            m_LeftPanelWidth = std::clamp(
                m_LeftPanelWidth + ImGui::GetIO().MouseDelta.x,
                k_MinPanelSize,
                W - m_RightPanelWidth - k_MinPanelSize - 2.0f * k_SplitterThickness);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    void EditorGui::DrawRightSplitter(float availH, float W)
    {
        ImGui::InvisibleButton("##right_vsplit", ImVec2(k_SplitterThickness, availH));
        if (ImGui::IsItemActive())
        {
            m_RightPanelWidth = std::clamp(
                m_RightPanelWidth - ImGui::GetIO().MouseDelta.x,
                k_MinPanelSize,
                W - m_LeftPanelWidth - k_MinPanelSize - 2.0f * k_SplitterThickness);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    void EditorGui::DrawCenterColumn(Context::Context& context, float centerW, float availH, void* sceneViewTextureId)
    {
        const float sceneViewH = availH - k_ToolbarHeight - k_SplitterThickness - m_ConsolePanelHeight;

        m_SceneViewWidth  = static_cast<uint32_t>(std::max(1.0f, centerW));
        m_SceneViewHeight = static_cast<uint32_t>(std::max(1.0f, sceneViewH));

        // Zero window padding so children tile with no internal gaps
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("##center_col", ImVec2(centerW, availH), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        float cursorY = 0.0f;

        // Toolbar
        ImGui::SetCursorPosY(cursorY);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::BeginChild("##toolbar_child", ImVec2(centerW, k_ToolbarHeight), true);
        ImGui::PopStyleVar();
        DrawToolbar();
        ImGui::EndChild();
        cursorY += k_ToolbarHeight;

        // Scene view — renders the offscreen scene texture into this panel
        ImGui::SetCursorPosY(cursorY);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::BeginChild("##scene_view_child", ImVec2(centerW, sceneViewH), false,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (sceneViewTextureId)
            ImGui::Image(sceneViewTextureId, ImVec2(centerW, sceneViewH));
        ImGui::EndChild();
        cursorY += sceneViewH;

        // Console resize splitter
        ImGui::SetCursorPosY(cursorY);
        DrawConsoleSplitter(centerW, availH);
        cursorY += k_SplitterThickness;

        // Console
        ImGui::SetCursorPosY(cursorY);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
        ImGui::BeginChild("##console_child", ImVec2(centerW, m_ConsolePanelHeight), true);
        ImGui::PopStyleVar();
        m_ConsolePanel->Draw();
        ImGui::EndChild();

        ImGui::EndChild(); // center_col
    }

    void EditorGui::DrawConsoleSplitter(float centerW, float availH)
    {
        ImGui::InvisibleButton("##h_split", ImVec2(centerW, k_SplitterThickness));
        if (ImGui::IsItemActive())
        {
            m_ConsolePanelHeight = std::clamp(
                m_ConsolePanelHeight - ImGui::GetIO().MouseDelta.y,
                k_MinPanelSize,
                availH - k_ToolbarHeight - k_SplitterThickness - k_MinPanelSize);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }

    // ─── Main menu ───────────────────────────────────────────────────────────

    void EditorGui::DrawMainMenu(Context::Context& context)
    {
        auto& scene = context.GetEngineContext().GetScene();
        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))    { scene.ResetScene(); }
            if (ImGui::MenuItem("Rename")) { scene.RenameScene(); }
            if (ImGui::MenuItem("Open"))   { scene.Load(); }
            if (ImGui::MenuItem("Save"))   { scene.Save(); }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Create game object"))
                scene.CreateGameObject(m_SelectedEntity);

            ImGui::Separator();
            if (ImGui::BeginMenu("Add component"))
            {
                if (ImGui::MenuItem("Mesh component") && m_SelectedEntity.has_value())
                    scene.AddMeshComponent(m_SelectedEntity.value());
                if (ImGui::MenuItem("Render component") && m_SelectedEntity.has_value())
                    scene.AddRenderComponent(m_SelectedEntity.value());
                if (ImGui::MenuItem("Light component") && m_SelectedEntity.has_value())
                    scene.AddLightComponent(m_SelectedEntity.value());
                if (ImGui::MenuItem("Script component") && m_SelectedEntity.has_value())
                    scene.AddScriptComponent(m_SelectedEntity.value());
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Create material"))
                context.GetEngineContext().GetMaterialContainer().CreateNewMaterial();
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

    void EditorGui::DrawToolbar()
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

    void EditorGui::DrawSceneHierarchy(Context::Context& context)
    {
        auto& scene = context.GetEngineContext().GetScene();

        ImGui::SeparatorText(scene.GetSceneName().c_str());

        const ImVec2 fullWidth = ImVec2(-FLT_MIN, 0.0f);
        if (ImGui::Button("Create object", fullWidth))
            scene.CreateGameObject(m_SelectedEntity);

        if (ImGui::Button("Delete object", fullWidth))
        {
            if (m_SelectedEntity.has_value() &&
                m_SelectedEntity.value() != scene.GetRoot())
            {
                scene.DeleteGameObject(m_SelectedEntity.value());
                m_SelectedEntity.reset();
            }
        }

        ImGui::Separator();

        int index = 0;
        DrawHierarchyNode(context, scene.GetRoot(), index);
    }

    void EditorGui::DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index)
    {
        auto& scene     = context.GetEngineContext().GetScene();
        auto& component = scene.GetHierarchyComponent(entity);

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

    void EditorGui::DrawInspector(Context::Context& context)
    {
        ImGui::SeparatorText("Inspector");

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

    void EditorGui::DrawEntityTitle(Context::Context& context)
    {
        auto& scene     = context.GetEngineContext().GetScene();
        auto  entity    = m_SelectedEntity.value();
        auto& hierarchy = scene.GetHierarchyComponent(entity);
        auto  name      = hierarchy.m_Name;

        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::InputText("##name", &name[0], name.capacity() + 1))
                hierarchy.m_Name = name;
        }
    }

    void EditorGui::DrawTransformComponent(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedEntity.value();

        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& transform = scene.GetTransformComponent(entity);

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

    void EditorGui::DrawMeshComponent(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedEntity.value();

        if (!scene.HasMeshComponent(entity))
            return;
        if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto&    mesh          = scene.GetMeshComponent(entity);
        auto&    meshPaths     = scene.GetMeshes();
        uint64_t selectedIndex = mesh.m_MeshIndex.value_or(0);

        if (ImGui::BeginCombo("mesh", mesh.m_MeshPath.c_str()))
        {
            for (uint64_t i = 0; i < meshPaths.size(); ++i)
            {
                const bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(meshPaths[i].c_str(), isSelected))
                    scene.ChangeMeshComponent(entity, meshPaths[i]);
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Load mesh"))
            scene.LoadMesh(entity);
        if (ImGui::Button("Remove mesh"))
            scene.RemoveMeshComponent(entity);
    }

    void EditorGui::DrawRenderComponent(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedEntity.value();

        if (!scene.HasRenderComponent(entity))
            return;
        if (!ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& render    = scene.GetRenderComponent(entity);
        auto& materials = scene.GetMaterials();

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
                        scene.UpdateMaterialComponent(entity);
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Load material"))
            scene.LoadMaterial(entity);

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            if (ImGui::Button("Save material"))
                context.GetEngineContext().GetMaterialContainer().SaveMaterial(
                    render.m_MaterialIndex.value());
        }

        if (ImGui::Button("Remove render"))
            scene.RemoveRenderComponent(entity);

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            ImGui::SeparatorText("Material");
            auto& materialContainer = context.GetEngineContext().GetMaterialContainer();
            auto& textureContainer  = context.GetEngineContext().GetTextureContainer();
            auto& materialData      = materialContainer.GetMaterialDataByIndex(
                render.m_MaterialIndex.value());

            const char* typeNames[] = { "Opaque", "Cutoff", "Transparent" };
            int materialType = static_cast<int>(materialData.type);
            if (ImGui::Combo("Type", &materialType, typeNames, IM_ARRAYSIZE(typeNames)))
                materialData.type = static_cast<Context::MaterialType>(materialType);

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

            if (materialData.type == Context::MaterialType::Transparent)
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

    void EditorGui::DrawLightComponent(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedEntity.value();

        if (!scene.HasLightComponent(entity))
            return;
        if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& light = scene.GetLightComponent(entity);

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
                scene.UpdateShadowCasting(entity, castShadows);
        }

        if (ImGui::Button("Remove light"))
            scene.RemoveLightComponent(entity);
    }

    void EditorGui::DrawScriptComponent(Context::Context& context)
    {
        auto& scene  = context.GetEngineContext().GetScene();
        auto  entity = m_SelectedEntity.value();

        if (!scene.HasScriptComponent(entity))
            return;
        if (!ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& component = scene.GetScriptComponent(entity);

        bool enabled = component.m_Enable;
        if (ImGui::Checkbox("Enabled", &enabled))
            component.m_NewEnable = enabled;

        std::string scriptName = component.m_ScriptPath.empty()
            ? "No script selected" : component.m_ScriptPath;
        ImGui::InputText("Script", &scriptName[0], scriptName.capacity() + 1);

        if (ImGui::Button("Load script"))
            scene.ChangeScript(entity);

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
            scene.RemoveScriptComponent(entity);
    }

    // ─── Settings window ─────────────────────────────────────────────────────

    void EditorGui::DrawSettingsWindow(Context::Context& context)
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
            ImGui::ColorEdit3("Panel background", m_PanelBgColor);
        }

        auto& settings = context.GetEngineContext().GetSettings();

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
            const auto& shadowDir = context.GetEngineContext().GetScene().GetShadowLightDirection();
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
