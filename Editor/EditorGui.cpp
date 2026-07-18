#include "EditorGui.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Tools/VertexDescriptionWindow.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/ShaderWindow.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "Reflection/GuiReflection.hpp"

#include "DialogueWindows/api/MaterialDialogue.hpp"
#include "DialogueWindows/api/MeshDialogue.hpp"
#include "DialogueWindows/api/SceneDialogue.hpp"
#include "DialogueWindows/api/ScriptDialogue.hpp"
#include "DialogueWindows/api/TextureDialogue.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <string_view>

namespace
{
    constexpr std::string_view ASSETS_PREFIX = "assets://";

    void SetupLightComponentGuiOverrides()
    {
        using HedgehogEngine::LightComponent;
        using HedgehogEngine::LightType;

        for (auto& prop : LightComponent::_GetPropTable())
        {
            if (std::string_view(prop.name) == "LightRadius")
            {
                prop.guiOverride = [](void* comp, const Reflection::PropertyDescriptor& p) -> bool
                {
                    auto* l = static_cast<LightComponent*>(comp);
                    if (l->m_LightType == LightType::DirectionLight) return false;
                    return ImGui::SliderFloat(p.name, &l->m_Radius, p.sliderMin, p.sliderMax);
                };
            }
            else if (std::string_view(prop.name) == "LightConeAngle")
            {
                prop.guiOverride = [](void* comp, const Reflection::PropertyDescriptor& p) -> bool
                {
                    auto* l = static_cast<LightComponent*>(comp);
                    if (l->m_LightType != LightType::SpotLight) return false;
                    return ImGui::SliderFloat(p.name, &l->m_ConeAngle, p.sliderMin, p.sliderMax);
                };
            }
            else if (std::string_view(prop.name) == "CastShadows")
            {
                prop.guiOverride = [](void*, const Reflection::PropertyDescriptor&) -> bool { return false; };
            }
        }
    }
}

namespace Editor
{
    EditorGui::EditorGui(HedgehogEngine::Engine& context)
        : m_ConsolePanel(std::make_unique<ConsolePanel>())
        , m_VertexDescWindow(std::make_unique<VertexDescriptionWindow>())
        , m_PipelineWindow(std::make_unique<PipelineWindow>())
        , m_ShaderWindow(std::make_unique<ShaderWindow>())
    {
        m_FileSystem = &context.GetEngineContext().GetFileSystem();
        if (m_Settings.Load("engine://editor_settings.yaml", *m_FileSystem)
            && m_Settings.dockLayout.IsValid())
            m_DockSystem.GetLayout() = m_Settings.dockLayout;

        SetupLightComponentGuiOverrides();

        loadLastScene(context);
    }

    EditorGui::~EditorGui()
    {
        m_Settings.dockLayout = m_DockSystem.GetLayout();
        // m_FileSystem is non-owning; the engine context (and thus FileSystemManager) is still
        // alive here because EditorGui is destroyed first among the Application's members.
        if (m_FileSystem)
            m_Settings.Save("engine://editor_settings.yaml", *m_FileSystem);
    }

    // ─── Top-level entry ─────────────────────────────────────────────────────

    void EditorGui::Draw(HedgehogEngine::Engine& context, void* sceneViewTextureId)
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

        const auto& fs = context.GetEngineContext().GetFileSystem();
        DrawSettingsWindow(context);
        m_VertexDescWindow->Draw(fs);
        m_PipelineWindow->Draw(fs);
        m_ShaderWindow->Draw(fs);
    }

    // ─── Panel dispatch ───────────────────────────────────────────────────────

    void EditorGui::DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context, void* sceneViewTextureId)
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

    void EditorGui::DrawMainMenu(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto* renderSystem  = engineContext.GetRenderSystem();

        if (!ImGui::BeginMainMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {
                engineContext.GetSceneManager().ResetScene();
                m_SelectedEntity.reset();
            }
            if (ImGui::MenuItem("Rename"))
            {
                char* newName = DialogueWindows::SceneRenameDialogue();
                if (newName != nullptr)
                    engineContext.GetSceneManager().SetSceneName(newName);
            }
            if (ImGui::MenuItem("Open"))
            {
                char* path = DialogueWindows::SceneOpenDialogue();
                if (path != nullptr)
                {
                    if (engineContext.GetSceneManager().LoadScene(path))
                    {
                        recordLastScene(path, engineContext.GetFileSystem());
                        m_SelectedEntity.reset();
                    }
                }
            }
            if (ImGui::MenuItem("Save"))
            {
                char* path = DialogueWindows::SceneSaveDialogue();
                if (path != nullptr)
                {
                    if (engineContext.GetSceneManager().SaveScene(path))
                        recordLastScene(path, engineContext.GetFileSystem());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::MenuItem("Create game object"))
                engineContext.GetSceneManager().CreateGameObject(m_SelectedEntity);

            ImGui::Separator();
            if (ImGui::BeginMenu("Add component"))
            {
                if (ImGui::MenuItem("Mesh component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<HedgehogEngine::MeshComponent>(e))
                    {
                        ecs.AddComponent(e, HedgehogEngine::MeshComponent{ HedgehogEngine::MeshSystem::sDefaultMeshPath });
                        meshSystem->Update(ecs, e, engineContext.GetFileSystem());
                    }
                }
                if (ImGui::MenuItem("Render component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<HedgehogEngine::RenderComponent>(e))
                    {
                        ecs.AddComponent(e, HedgehogEngine::RenderComponent{});
                        renderSystem->Update(ecs, e);
                    }
                }
                if (ImGui::MenuItem("Light component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<HedgehogEngine::LightComponent>(e))
                        ecs.AddComponent(e, HedgehogEngine::LightComponent{});
                }
                if (ImGui::MenuItem("Script component") && m_SelectedEntity.has_value())
                {
                    ECS::Entity e = m_SelectedEntity.value();
                    if (!ecs.HasComponent<HedgehogEngine::ScriptComponent>(e))
                        ecs.AddComponent(e, HedgehogEngine::ScriptComponent{});
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Create material"))
            {
                if (const char* path = DialogueWindows::MaterialCreationDialogue())
                {
                    const auto& fs = engineContext.GetFileSystem();
                    const auto virtualPath = fs.ToVirtualPath(path);
                    if (virtualPath)
                        engineContext.GetResourceCatalog().GetMaterialContainer().CreateNewMaterial(fs, *virtualPath);
                    else
                        LOGERROR("Material path is not under any registered mount: ", path);
                }
            }
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

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Vertex Descriptions", nullptr, m_VertexDescWindow->m_Open))
                m_VertexDescWindow->m_Open = !m_VertexDescWindow->m_Open;
            if (ImGui::MenuItem("Pipeline", nullptr, m_PipelineWindow->m_Open))
                m_PipelineWindow->m_Open = !m_PipelineWindow->m_Open;
            if (ImGui::MenuItem("Shader", nullptr, m_ShaderWindow->m_Open))
                m_ShaderWindow->m_Open = !m_ShaderWindow->m_Open;
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

    void EditorGui::DrawSceneHierarchy(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& sceneManager  = engineContext.GetSceneManager();

        ImGui::SeparatorText(sceneManager.GetSceneName().c_str());

        const ImVec2 fullWidth = ImVec2(-FLT_MIN, 0.0f);
        if (ImGui::Button("Create object", fullWidth))
            sceneManager.CreateGameObject(m_SelectedEntity);

        if (ImGui::Button("Delete object", fullWidth))
        {
            if (m_SelectedEntity.has_value() && m_SelectedEntity.value() != sceneManager.GetRootEntity())
            {
                sceneManager.DeleteGameObject(m_SelectedEntity.value());
                m_SelectedEntity.reset();
            }
        }

        ImGui::Separator();

        int index = 0;
        DrawHierarchyNode(context, sceneManager.GetRootEntity(), index);
    }

    void EditorGui::DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, int& index)
    {
        auto& ecs       = context.GetEngineContext().GetECS();
        auto& component = ecs.GetComponent<ECS::HierarchyComponent>(entity);

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

    void EditorGui::DrawInspector(HedgehogEngine::Engine& context)
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

    void EditorGui::DrawEntityTitle(HedgehogEngine::Engine& context)
    {
        auto& ecs       = context.GetEngineContext().GetECS();
        auto  entity    = m_SelectedEntity.value();
        auto& hierarchy = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
        {
            char nameBuf[256];
            strncpy_s(nameBuf, hierarchy.m_Name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
                hierarchy.m_Name = nameBuf;
        }
    }

    void EditorGui::DrawTransformComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto  entity        = m_SelectedEntity.value();

        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);

        if (Reflection::RenderComponentGui(&transform, HedgehogEngine::TransformComponent::GetProperties()))
            engineContext.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
    }

    void EditorGui::DrawMeshComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::MeshComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto&    mesh          = ecs.GetComponent<HedgehogEngine::MeshComponent>(entity);
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
                    meshSystem->Update(ecs, entity, engineContext.GetFileSystem());
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Load mesh"))
        {
            if (const char* path = DialogueWindows::MeshOpenDialogue())
            {
                const auto& fs = engineContext.GetFileSystem();
                const auto virtualPath = fs.ToVirtualPath(path);
                if (virtualPath)
                {
                    meshSystem->LoadMesh(ecs, entity, virtualPath->substr(ASSETS_PREFIX.size()));
                }
                else
                {
                    LOGERROR("Mesh path is not under any registered mount: ", path);
                }
            }
        }
        if (ImGui::Button("Remove mesh"))
        {
            if (ecs.HasComponent<HedgehogEngine::MeshComponent>(entity))
                ecs.RemoveComponent<HedgehogEngine::MeshComponent>(entity);
        }
    }

    void EditorGui::DrawRenderComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* renderSystem  = engineContext.GetRenderSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::RenderComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& render        = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
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
                const auto virtualPath = engineContext.GetFileSystem().ToVirtualPath(path);
                if (virtualPath)
                {
                    // Strip "assets://" prefix; m_Material stores the relative path.
                    render.m_Material = virtualPath->substr(ASSETS_PREFIX.size());
                    renderSystem->Update(ecs, entity);
                }
                else
                {
                    LOGERROR("EditorGui: Load material path is not under any registered mount (path: ", path, ")");
                }
            }
        }

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            if (ImGui::Button("Save material"))
                engineContext.GetResourceCatalog().GetMaterialContainer().SaveMaterial(
                    render.m_MaterialIndex.value(), engineContext.GetFileSystem());
        }

        if (ImGui::Button("Remove render"))
        {
            if (ecs.HasComponent<HedgehogEngine::RenderComponent>(entity))
                ecs.RemoveComponent<HedgehogEngine::RenderComponent>(entity);
        }

        if (!materials.empty() && render.m_MaterialIndex.has_value())
        {
            ImGui::SeparatorText("Material");
            auto& materialContainer = engineContext.GetResourceCatalog().GetMaterialContainer();
            auto& textureContainer  = engineContext.GetResourceCatalog().GetTextureContainer();
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
            {
                if (const char* texPath = DialogueWindows::TextureOpenDialogue())
                {
                    const auto& fs = engineContext.GetFileSystem();
                    const auto virtualPath = fs.ToVirtualPath(texPath);
                    if (virtualPath)
                    {
                        materialContainer.LoadBaseTexture(render.m_MaterialIndex.value(),
                                                          virtualPath->substr(ASSETS_PREFIX.size()));
                    }
                    else
                    {
                        LOGERROR("Texture path is not under any registered mount: ", texPath);
                    }
                }
            }

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

    void EditorGui::DrawLightComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* lightSystem   = engineContext.GetLightSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::LightComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& light = ecs.GetComponent<HedgehogEngine::LightComponent>(entity);

        Reflection::RenderComponentGui(&light, HedgehogEngine::LightComponent::GetProperties());

        if (light.m_LightType == HedgehogEngine::LightType::DirectionLight)
        {
            bool castShadows = light.m_CastShadows;
            if (ImGui::Checkbox("Cast shadows", &castShadows))
                lightSystem->SetShadowCasting(ecs, entity, castShadows);
        }

        if (ImGui::Button("Remove light"))
        {
            if (ecs.HasComponent<HedgehogEngine::LightComponent>(entity))
                ecs.RemoveComponent<HedgehogEngine::LightComponent>(entity);
        }
    }

    void EditorGui::DrawScriptComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* scriptSystem  = engineContext.GetScriptSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
            return;
        if (!ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);

        bool enabled = component.m_Enable;
        if (ImGui::Checkbox("Enabled", &enabled))
            component.m_NewEnable = enabled;

        {
            const std::string& scriptSrc = component.m_ScriptPath.empty()
                ? "No script selected" : component.m_ScriptPath;
            char scriptBuf[256];
            strncpy_s(scriptBuf, scriptSrc.c_str(), sizeof(scriptBuf) - 1);
            scriptBuf[sizeof(scriptBuf) - 1] = '\0';
            ImGui::InputText("Script", scriptBuf, sizeof(scriptBuf));
        }

        if (ImGui::Button("Load script"))
        {
            std::string scriptPath = DialogueWindows::ScriptChooseDialogue();
            if (!scriptPath.empty())
                scriptSystem->ChangeScript(entity, ecs, engineContext.GetEventBus(),
                                           engineContext.GetFileSystem(), scriptPath);
        }

        if (!component.m_Params.empty())
            ImGui::SeparatorText("Parameters");

        for (auto& [name, param] : component.m_Params)
        {
            switch (param.type)
            {
            case HedgehogEngine::ParamType::Boolean:
            {
                bool bVal = std::get<bool>(param.value);
                if (ImGui::Checkbox(name.c_str(), &bVal))
                {
                    param.value = bVal;
                    param.dirty = true;
                }
                break;
            }
            case HedgehogEngine::ParamType::Number:
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
            if (ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
                ecs.RemoveComponent<HedgehogEngine::ScriptComponent>(entity);
        }
    }

    // ─── Settings window ─────────────────────────────────────────────────────

    void EditorGui::DrawSettingsWindow(HedgehogEngine::Engine& context)
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
                m_Settings.Save("engine://editor_settings.yaml",
                                context.GetEngineContext().GetFileSystem());
            ImGui::SameLine();
            if (ImGui::Button("Load settings"))
                m_Settings.Load("engine://editor_settings.yaml",
                                context.GetEngineContext().GetFileSystem());
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

    // ─── Last-scene persistence ──────────────────────────────────────────────

    void EditorGui::recordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem)
    {
        const auto virtualPath = fileSystem.ToVirtualPath(nativePath);
        if (!virtualPath)
        {
            LOGERROR("EditorGui::recordLastScene: path is not under any registered mount (path: ", nativePath, ")");
            return;
        }

        m_Settings.m_LastScene = *virtualPath;
        m_Settings.Save("engine://editor_settings.yaml", fileSystem);
    }

    void EditorGui::loadLastScene(HedgehogEngine::Engine& context)
    {
        if (m_Settings.m_LastScene.empty())
            return;

        if (!m_FileSystem->Exists(m_Settings.m_LastScene))
        {
            LOGWARNING("EditorGui::loadLastScene: stored scene not found, starting with an empty scene (path: ",
                       m_Settings.m_LastScene, ")");
            m_Settings.m_LastScene.clear();
            m_Settings.Save("engine://editor_settings.yaml", *m_FileSystem);
            return;
        }

        // LoadScene expects a native path (the dialog's contract), so resolve back from virtual.
        const auto physicalPath = m_FileSystem->ResolvePhysical(m_Settings.m_LastScene);
        if (physicalPath && context.GetEngineContext().GetSceneManager().LoadScene(physicalPath->string()))
            return;

        LOGWARNING("EditorGui::loadLastScene: failed to load stored scene, starting with an empty scene (path: ",
                   m_Settings.m_LastScene, ")");
        context.GetEngineContext().GetSceneManager().ResetScene();
        m_Settings.m_LastScene.clear();
        m_Settings.Save("engine://editor_settings.yaml", *m_FileSystem);
    }
}
