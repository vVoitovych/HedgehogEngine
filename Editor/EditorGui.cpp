#include "EditorGui.hpp"
#include "EditorTheme.hpp"
#include "Widgets/IconWidgets.hpp"
#include "Widgets/PropertyFields.hpp"
#include "Widgets/TransformGizmo.hpp"
#include "Widgets/ViewportOverlay.hpp"
#include "Panels/EntityIcon.hpp"
#include "Panels/TextSearch.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Panels/ContentPanel.hpp"
#include "Tools/VertexDescriptionWindow.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/ShaderWindow.hpp"
#include "Tools/InputActionsWindow.hpp"
#include "Tools/BuildGameWindow.hpp"
#include "Tools/NewProjectWindow.hpp"
#include "Tools/ProjectSettingsWindow.hpp"
#include "Panels/AssetDragDrop.hpp"
#include "Panels/EntityDragDrop.hpp"
#include "Panels/MaterialFields.hpp"
#include "Panels/ScriptPropertyFields.hpp"
#include "Panels/PhysicsSettingsFields.hpp"
#include "Tools/RenderGraphEditor/GraphFileReference.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorWindow.hpp"

#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/api/Containers/MaterialContainer.hpp"
#include "HedgehogEngine/api/Containers/MaterialData.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/TextureContainer.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "Project/StartupProject.hpp"
#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiButtonComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiCanvasComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiImageComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioListenerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiRectComponent.hpp"
#include "HedgehogEngine/api/ECS/components/UiTextComponent.hpp"
#include "Reflection/GuiReflection.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "DialogueWindows/api/MaterialDialogue.hpp"
#include "DialogueWindows/api/MeshDialogue.hpp"
#include "DialogueWindows/api/PrefabDialogue.hpp"
#include "DialogueWindows/api/ProjectDialogue.hpp"
#include "DialogueWindows/api/RenderGraphDialogue.hpp"
#include "DialogueWindows/api/SceneDialogue.hpp"
#include "DialogueWindows/api/ScriptDialogue.hpp"
#include "DialogueWindows/api/TextureDialogue.hpp"

#include "HedgehogRenderer/Graph/GraphReference.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::string_view ASSETS_PREFIX = "assets://";

    void SetupLightComponentGuiOverrides()
    {
        using HedgehogEngine::LightComponent;
        using HedgehogEngine::LightType;

        for (auto& prop : LightComponent::GetPropTable_())
        {
            if (std::string_view(prop.name) == "LightRadius")
            {
                prop.guiOverride = [](void* comp, const Reflection::PropertyDescriptor& p) -> bool
                {
                    auto* l = static_cast<LightComponent*>(comp);
                    if (l->LightType == LightType::DirectionLight) return false;
                    Editor::PropertyLabel(p.name);
                    return ImGui::SliderFloat("##LightRadius", &l->Radius, p.sliderMin, p.sliderMax);
                };
            }
            else if (std::string_view(prop.name) == "LightConeAngle")
            {
                prop.guiOverride = [](void* comp, const Reflection::PropertyDescriptor& p) -> bool
                {
                    auto* l = static_cast<LightComponent*>(comp);
                    if (l->LightType != LightType::SpotLight) return false;
                    Editor::PropertyLabel(p.name);
                    return ImGui::SliderFloat("##LightConeAngle", &l->ConeAngle, p.sliderMin, p.sliderMax);
                };
            }
            else if (std::string_view(prop.name) == "CastShadows" || std::string_view(prop.name) == "LightEnabled")
            {
                // Cast shadows goes through the light system; Enable is the header's checkbox.
                prop.guiOverride = [](void*, const Reflection::PropertyDescriptor&) -> bool { return false; };
            }
        }
    }

    // LayerMask stays reflected (None, not Hidden) so it still serialises — Hidden properties
    // are skipped by YamlSerializeComponent/YamlDeserializeComponent, not just the GUI. The
    // override below only silences the generic DragInt widget so DrawCameraComponent can draw
    // named checkboxes instead, sourced live from HedgehogSettings::LayerSettings.

    void SetupCameraComponentGuiOverrides()
    {
        using HedgehogEngine::CameraComponent;

        for (auto& prop : CameraComponent::GetPropTable_())
        {
            // GraphName likewise: DrawCameraGraph lists the renderer's graphs, with Browse... and
            // Edit, instead of a free-text field. Enabled is the camera header's checkbox.
            const std::string_view name(prop.name);
            if (name == "LayerMask" || name == "GraphName" || name == "Enabled")
            {
                prop.guiOverride = [](void*, const Reflection::PropertyDescriptor&) -> bool { return false; };
            }
        }
    }
}

namespace Editor
{
    namespace
    {
        // What the entity has that decides its icon.
        [[nodiscard]] EntityTraits MakeEntityTraits(ECS::ECS& ecs, ECS::Entity entity)
        {
            EntityTraits traits;
            traits.HasCamera        = ecs.HasComponent<HedgehogEngine::CameraComponent>(entity);
            traits.HasLight         = ecs.HasComponent<HedgehogEngine::LightComponent>(entity);
            traits.HasUiCanvas      = ecs.HasComponent<HedgehogEngine::UiCanvasComponent>(entity);
            traits.HasUiElement     = ecs.HasComponent<HedgehogEngine::UiRectComponent>(entity);
            traits.HasAudioSource   = ecs.HasComponent<HedgehogEngine::AudioSourceComponent>(entity);
            traits.HasAudioListener = ecs.HasComponent<HedgehogEngine::AudioListenerComponent>(entity);
            traits.HasMesh          = ecs.HasComponent<HedgehogEngine::MeshComponent>(entity);
            traits.HasChildren      = !ecs.GetComponent<ECS::HierarchyComponent>(entity).Children.empty();
            return traits;
        }

        // Spaces at the start of a hierarchy row's label, where its icon is drawn.
        constexpr const char* HIERARCHY_ICON_PAD = "        ";
    }

    EditorGui::EditorGui(HedgehogEngine::Engine& context, bool recordRecentProject)
        : m_RecordRecentProject(recordRecentProject)
        , m_ConsolePanel(std::make_unique<ConsolePanel>())
        , m_VertexDescWindow(std::make_unique<VertexDescriptionWindow>())
        , m_PipelineWindow(std::make_unique<PipelineWindow>())
        , m_ShaderWindow(std::make_unique<ShaderWindow>())
        , m_InputActionsWindow(std::make_unique<InputActionsWindow>())
        , m_ProjectSettingsWindow(std::make_unique<ProjectSettingsWindow>())
        , m_NewProjectWindow(std::make_unique<NewProjectWindow>())
        , m_BuildGameWindow(std::make_unique<BuildGameWindow>())
        , m_RenderGraphEditorWindow(std::make_unique<RenderGraphEditorWindow>())
    {
        m_FileSystem   = &context.GetEngineContext().GetFileSystem();
        m_ContentPanel = std::make_unique<ContentPanel>(*m_FileSystem);
        // A first run, with no file yet, keeps the default layout.
        if (m_Settings.Load(EditorSettings::PATH, *m_FileSystem)
            && m_Settings.dockLayout.IsValid())
            m_DockSystem.GetLayout() = m_Settings.dockLayout;
        m_ContentPanel->SetIconSize(m_Settings.ContentIconSize);
        // The open project goes to the front of the recent list, keeping the scene it last had open.
        // Entries whose folder is gone stay, shown disabled, until File > Recent Projects clears them.
        m_ProjectRoot = FS::GetProjectRootDirectory();
        if (m_RecordRecentProject)
            (void)TouchRecentProject(m_Settings.RecentProjects, m_ProjectRoot);


        SetupLightComponentGuiOverrides();
        SetupCameraComponentGuiOverrides();
        // Hand-drawn sections by component key; every other registered component gets the generic
        // reflected section, with these extra rows after its own.
        m_InspectorDrawers = {
            { "TransformComponent", &EditorGui::DrawTransformComponent },
            { "LightComponent", &EditorGui::DrawLightComponent },
            { "CameraComponent", &EditorGui::DrawCameraComponent },
            { "MeshComponent", &EditorGui::DrawMeshComponent },
            { "RenderComponent", &EditorGui::DrawRenderComponent },
            { "ScriptComponent", &EditorGui::DrawScriptComponent },
            { "AnimatorComponent", &EditorGui::DrawAnimatorComponent },
        };
        m_InspectorAssetDrops = {
            { "UiTextComponent", { "Font", ContentType::Font, "a .ttf or .otf font" } },
            { "AudioSourceComponent", { "Clip", ContentType::Audio, "a .wav, .mp3 or .flac clip" } },
            { "EnvironmentComponent", { "Map", ContentType::Environment, "an .hdr environment map" } },
        };

        LoadLastScene(context);
    }

    EditorGui::~EditorGui()
    {
        m_Settings.dockLayout      = m_DockSystem.GetLayout();
        m_Settings.ContentIconSize = m_ContentPanel->GetIconSize();
        // m_FileSystem is non-owning; the engine context (and thus FileSystemManager) is still
        // alive here because EditorGui is destroyed first among the Application's members.
        if (m_FileSystem)
            m_Settings.Save(EditorSettings::PATH, *m_FileSystem);
    }

    // ─── Top-level entry ─────────────────────────────────────────────────────

    void EditorGui::Draw(HedgehogEngine::Engine& context, const ViewportImages& images)
    {
        BeginTransformGizmoFrame();
        m_SceneViewHovered = false;
        m_SceneGizmoActive = false;
        m_ScenePick.reset();
        m_SceneViewWidth   = 0;
        m_SceneViewHeight  = 0;
        m_GameViewWidth    = 0;
        m_GameViewHeight   = 0;
        m_GameImageSize    = HM::Vector2(0.0f, 0.0f);
        m_GameViewHovered  = false;
        m_SceneImageSize   = HM::Vector2(0.0f, 0.0f);

        m_ViewportImages = images;

        DrawMainMenu(context);

        const float menuH = ImGui::GetFrameHeight();

        DockPanelIcons panelIcons;
        panelIcons.Panels[static_cast<size_t>(PanelId::SceneHierarchy)] = GetIcon(EditorIcon::Hierarchy);
        panelIcons.Panels[static_cast<size_t>(PanelId::Inspector)]      = GetIcon(EditorIcon::Inspector);
        panelIcons.Panels[static_cast<size_t>(PanelId::Console)]        = GetIcon(EditorIcon::Console);
        panelIcons.Panels[static_cast<size_t>(PanelId::Content)]        = GetIcon(EditorIcon::Project);
        panelIcons.More                                                 = GetIcon(EditorIcon::More);

        m_DockSystem.Draw(
            [this, &context]() { DrawToolbarContent(context); },
            [this, &context](PanelId panel) { DrawPanelContent(panel, context); },
            menuH, panelIcons);

        const auto& fs = context.GetEngineContext().GetFileSystem();
        DrawSettingsWindow(context);
        m_VertexDescWindow->Draw(fs);
        m_PipelineWindow->Draw(fs);
        m_ShaderWindow->Draw(fs);
        m_InputActionsWindow->Draw(fs, context.GetWindowContext().GetWindow().GetRawInput());
        m_ProjectSettingsWindow->Draw(context.GetEngineContext());
        m_BuildGameWindow->Draw();
        // A created project is opened at once: the window already said the scene's changes go.
        if (const auto created = m_NewProjectWindow->Draw(context.GetEngineContext().GetPlayState() ==
                                                          HedgehogEngine::PlayState::Edit))
            RequestProject(*created, false);
        m_RenderGraphEditorWindow->Draw(m_Renderer, *m_FileSystem);

        DrawOpenSceneDropPopup(context);
        ApplyAssetDrop(context);
    }

    // ─── Panel dispatch ───────────────────────────────────────────────────────

    void EditorGui::DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context)
    {
        switch (panel)
        {
        case PanelId::SceneHierarchy: DrawSceneHierarchy(context);          break;
        case PanelId::Inspector:      DrawInspector(context);                break;
        case PanelId::Console:        m_ConsolePanel->Draw(m_MonoFont);      break;
        case PanelId::Content:        DrawContentPanel(context);             break;
        default:                      DrawSceneViewContent(context);         break;
        }
    }

    void EditorGui::DrawContentPanel(HedgehogEngine::Engine& context)
    {
        if (m_Benchmarking)
            ImGui::TextDisabled("Hidden while benchmarking.");
        else
        {
            ContentPanelIcons icons;
            icons.Types  = m_ViewportImages.ContentIcons;
            icons.Folder = GetIcon(EditorIcon::Folder);
            icons.Plus   = GetIcon(EditorIcon::Plus);
            const ContentPanelRequest request = m_ContentPanel->Draw(icons);
            if (request.Open)
                OpenContentItem(context, *request.Open);
            if (request.CreateMaterial)
                CreateMaterial(context.GetEngineContext());
            if (request.ImportMaterials)
                (void)ImportMaterials(context.GetEngineContext(), *request.ImportMaterials);
        }
    }

    void EditorGui::DrawSceneViewContent(HedgehogEngine::Engine& context)
    {
        if (!ImGui::BeginTabBar("##SceneGameTabs"))
            return;

        // A tab's label leaves room for its icon, drawn over it.
        const auto drawTabIcon = [this](EditorIcon icon)
        {
            const ImVec2 min = ImGui::GetItemRectMin();
            const float  y   = min.y + (ImGui::GetItemRectSize().y - ICON_SIZE_SMALL) * 0.5f;
            DrawIcon(*ImGui::GetWindowDrawList(), GetIcon(icon), ImVec2(min.x + ImGui::GetStyle().FramePadding.x, y),
                     ICON_SIZE_SMALL, ImGui::GetColorU32(ImGuiCol_Text));
        };

        // Only the visible tab gets a size: the hidden one stays 0x0, so its view renders nothing.
        const bool sceneOpen = ImGui::BeginTabItem("      Scene###Scene");
        drawTabIcon(EditorIcon::Scene);
        if (sceneOpen)
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            m_SceneViewWidth  = static_cast<uint32_t>(std::max(1.0f, avail.x));
            m_SceneViewHeight = static_cast<uint32_t>(std::max(1.0f, avail.y));

            if (m_ViewportImages.Scene)
            {
                ImGui::Image(m_ViewportImages.Scene, avail);
                m_SceneViewHovered = ImGui::IsItemHovered();
                m_SceneImageMin    = HM::Vector2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y);
                m_SceneImageSize   = HM::Vector2(ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);

                // The selection's handles, over the image; one under the pointer takes it from the
                // camera and from picking.
                m_SceneGizmoActive = DrawSceneGizmo(context, m_SceneImageMin, m_SceneImageSize);

                // A click, not the end of a camera drag or on a handle.
                constexpr float CLICK_DRAG_PIXELS = 4.0f;
                const ImGuiIO&  io = ImGui::GetIO();
                if (m_SceneViewHovered && !m_SceneGizmoActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
                    && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < CLICK_DRAG_PIXELS * CLICK_DRAG_PIXELS)
                {
                    const ImVec2 origin = ImGui::GetItemRectMin();
                    const ImVec2 size   = ImGui::GetItemRectSize();
                    m_ScenePick = ViewportPoint{ (io.MousePos.x - origin.x) / size.x, (io.MousePos.y - origin.y) / size.y };
                }
            }
            // Overlays over the image: the axes as the editor camera sees them, and the pass count.
            if (m_ViewportImages.Scene)
            {
                const ImVec2 imageMin  = ImGui::GetItemRectMin();
                const ImVec2 imageSize = ImGui::GetItemRectSize();
                ImDrawList&  drawList  = *ImGui::GetWindowDrawList();
                DrawAxisGizmo(drawList, imageMin, imageSize, context.GetEngineContext().GetCamera().GetViewMatrix());
                if (m_ViewportImages.GraphPassCount > 0)
                    DrawPassCount(drawList, imageMin, imageSize, m_ViewportImages.GraphPassCount);
            }

            ImGui::EndTabItem();
        }

        const bool gameOpen = ImGui::BeginTabItem("      Game###Game");
        drawTabIcon(EditorIcon::Game);
        if (gameOpen)
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            m_GameViewWidth  = static_cast<uint32_t>(std::max(1.0f, avail.x));
            m_GameViewHeight = static_cast<uint32_t>(std::max(1.0f, avail.y));

            if (m_ViewportImages.Game)
            {
                ImGui::Image(m_ViewportImages.Game, avail);
                m_GameImageMin    = HM::Vector2(ImGui::GetItemRectMin().x, ImGui::GetItemRectMin().y);
                m_GameImageSize   = HM::Vector2(ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);
                m_GameViewHovered = ImGui::IsItemHovered();
            }
            else
            {
                ImGui::TextDisabled("No camera is drawn here: add an enabled camera to the scene.");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();

        // A press focuses a panel when it lands on its image, and unfocuses it anywhere else; a hidden
        // panel has no focus.
        const bool anyPress = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
                              ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        if (anyPress)
        {
            m_GameViewFocused  = m_GameViewHovered;
            m_SceneViewFocused = m_SceneViewHovered;
        }
        if (m_GameImageSize.x() <= 0.0f)
            m_GameViewFocused = false;
        if (m_SceneImageSize.x() <= 0.0f)
            m_SceneViewFocused = false;
        HandleTransformToolKeys();
    }

    HInput::GameInputRegion EditorGui::GetGameInputRegion() const
    {
        HInput::GameInputRegion region;
        region.Origin          = m_GameImageMin;
        region.Size            = m_GameImageSize;
        region.PixelSize       = HM::Vector2(static_cast<float>(m_GameViewWidth), static_cast<float>(m_GameViewHeight));
        region.PointerEnabled  = m_GameViewHovered;
        region.KeyboardEnabled = m_GameViewFocused && !ImGui::GetIO().WantTextInput;
        return region;
    }

    HInput::GameInputRegion EditorGui::GetSceneInputRegion() const
    {
        HInput::GameInputRegion region;
        region.Origin          = m_SceneImageMin;
        region.Size            = m_SceneImageSize;
        region.PixelSize       = m_SceneImageSize; // the camera turns by window pixels, as it always has
        region.PointerEnabled  = m_SceneViewHovered && !m_SceneGizmoActive; // a handle's drag is not the camera's
        region.KeyboardEnabled = m_SceneViewFocused && !ImGui::GetIO().WantTextInput;
        return region;
    }

    // ─── Main menu ───────────────────────────────────────────────────────────

    void EditorGui::DrawMainMenu(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();

        if (!ImGui::BeginMainMenuBar())
            return;

        // The logo and the engine's name lead the bar.
        if (void* logo = GetIcon(EditorIcon::Logo))
        {
            ImGui::Image(logo, ImVec2(ICON_SIZE_SMALL, ICON_SIZE_SMALL));
            ImGui::SameLine();
        }
        ImGui::TextUnformatted("HedgehogEngine");
        ImGui::Dummy(ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

        if (ImGui::BeginMenu("File"))
        {
            // Scene files change only in Edit: Stop would overwrite whatever they loaded.
            const bool editing = engineContext.GetPlayState() == HedgehogEngine::PlayState::Edit;
            if (ImGui::MenuItem("New", nullptr, false, editing))
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
            if (ImGui::MenuItem("Open", nullptr, false, editing))
            {
                char* path = DialogueWindows::SceneOpenDialogue();
                if (path != nullptr)
                {
                    if (engineContext.GetSceneManager().LoadScene(path))
                    {
                        RecordLastScene(path, engineContext.GetFileSystem());
                        m_SelectedEntity.reset();
                    }
                }
            }
            if (ImGui::MenuItem("Save", nullptr, false, editing))
            {
                char* path = DialogueWindows::SceneSaveDialogue();
                if (path != nullptr)
                {
                    if (engineContext.GetSceneManager().SaveScene(path))
                        RecordLastScene(path, engineContext.GetFileSystem());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Project Settings..."))
                m_ProjectSettingsWindow->Show(engineContext);
            // Packages the project as saved on disk, so Play does not get in its way.
            if (ImGui::MenuItem("Build Game..."))
                m_BuildGameWindow->Show(m_ProjectRoot, engineContext.GetSettings().GetProjectSettings().GetName());
            DrawProjectMenuItems(editing);
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Settings..."))
                m_SettingsWindowOpen = true;
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Assets"))
        {
            if (ImGui::MenuItem("Create material"))
                CreateMaterial(engineContext);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("GameObject"))
        {
            auto&      sceneManager = engineContext.GetSceneManager();
            const bool deletable    = m_SelectedEntity.has_value() && *m_SelectedEntity != sceneManager.GetRootEntity();
            if (ImGui::MenuItem("Create game object"))
                sceneManager.CreateGameObject(m_SelectedEntity);
            if (ImGui::MenuItem("Delete", nullptr, false, deletable))
            {
                sceneManager.DeleteGameObject(*m_SelectedEntity);
                m_SelectedEntity.reset();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Component", m_SelectedEntity.has_value()))
        {
            DrawAddComponentItems(context);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            auto& layout = m_DockSystem.GetLayout();
            for (int i = 0; i < PANEL_ID_COUNT; ++i)
            {
                const PanelId pid     = static_cast<PanelId>(i);
                const bool    visible = layout.PanelVisible[i];
                if (ImGui::MenuItem(PanelName(pid), nullptr, visible))
                {
                    if (visible)
                        layout.HidePanel(pid);
                    else
                        layout.ShowPanel(pid);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Vertex Descriptions", nullptr, m_VertexDescWindow->Open))
                m_VertexDescWindow->Open = !m_VertexDescWindow->Open;
            if (ImGui::MenuItem("Pipeline", nullptr, m_PipelineWindow->Open))
                m_PipelineWindow->Open = !m_PipelineWindow->Open;
            if (ImGui::MenuItem("Shader", nullptr, m_ShaderWindow->Open))
                m_ShaderWindow->Open = !m_ShaderWindow->Open;
            if (ImGui::MenuItem("Input Actions", nullptr, m_InputActionsWindow->Open))
            {
                if (m_InputActionsWindow->Open)
                    m_InputActionsWindow->Open = false;
                else
                    m_InputActionsWindow->OpenFile(context.GetEngineContext().GetFileSystem());
            }
            if (ImGui::MenuItem("Render Graph Editor", nullptr, m_RenderGraphEditorWindow->Open))
                m_RenderGraphEditorWindow->Open = !m_RenderGraphEditorWindow->Open;
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Asks where, then creates a material there (the Assets menu and the Project panel's "+").
    void EditorGui::CreateMaterial(HedgehogEngine::EngineContext& engineContext)
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

    void EditorGui::DrawAddComponentItems(HedgehogEngine::Engine& context)
    {
        auto&      engineContext = context.GetEngineContext();
        auto&      ecs           = engineContext.GetECS();
        const auto infos         = engineContext.GetComponentTypes().GetInfosInOrder();

        // Every addable component type as "<name> component", grouped by category in the order the
        // groups first appear, each group in SortOrder; one the selected entity has is greyed.
        std::vector<std::string_view> categories;
        for (const EcsSerialization::ComponentInfo* info : infos)
        {
            if (info->Addable && std::find(categories.begin(), categories.end(), info->Category) == categories.end())
                categories.push_back(info->Category);
        }
        for (size_t group = 0; group < categories.size(); ++group)
        {
            if (group > 0)
                ImGui::Separator();
            for (const EcsSerialization::ComponentInfo* info : infos)
            {
                if (!info->Addable || info->Category != categories[group])
                    continue;
                const std::string label  = info->DisplayName + " component";
                const bool        canAdd = m_SelectedEntity.has_value() && !info->Has(ecs, *m_SelectedEntity);
                if (ImGui::MenuItem(label.c_str(), nullptr, false, canAdd))
                    info->AddDefault(ecs, *m_SelectedEntity);
            }
        }
    }

    // ─── Toolbar ─────────────────────────────────────────────────────────────

    void EditorGui::DrawToolbarContent(HedgehogEngine::Engine& context)
    {
        auto&                           engineContext = context.GetEngineContext();
        const HedgehogEngine::PlayState state         = engineContext.GetPlayState();

        const bool isEdit  = (state == HedgehogEngine::PlayState::Edit);
        const bool isPlay  = (state == HedgehogEngine::PlayState::Playing);
        const bool isPause = (state == HedgehogEngine::PlayState::Paused);

        const ImGuiStyle& style      = ImGui::GetStyle();
        const float       buttonSize = ICON_SIZE_SMALL + 2.0f * style.FramePadding.x;
        const ImVec4      iconTint   = style.Colors[ImGuiCol_Text];
        const ImVec4      activeFill = Theme::Resolve(Theme::PLAY_TINT);

        // The play-mode buttons sit in the middle of the row; the one matching the state is filled.
        const auto playButton = [&](const char* id, EditorIcon icon, bool active, bool enabled, const char* tooltip)
        {
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, activeFill);
            ImGui::BeginDisabled(!enabled);
            const bool pressed = IconButton(id, GetIcon(icon), ICON_SIZE_SMALL, iconTint);
            ImGui::EndDisabled();
            if (active)
                ImGui::PopStyleColor();
            ImGui::SetItemTooltip("%s", tooltip);
            return pressed;
        };

        // The transform tools lead the row.
        DrawTransformToolButtons();
        ImGui::SameLine();

        const float groupWidth = 3.0f * buttonSize + 2.0f * style.ItemSpacing.x;
        ImGui::SetCursorPosX(std::max((ImGui::GetWindowWidth() - groupWidth) * 0.5f, style.WindowPadding.x));

        if (playButton("##Play", EditorIcon::Play, isPlay, isEdit, "Play"))
            (void)engineContext.Play();

        ImGui::SameLine();
        if (playButton("##Pause", EditorIcon::Pause, isPause, !isEdit, isPause ? "Resume" : "Pause"))
            (void)(isPause ? engineContext.Resume() : engineContext.Pause());

        ImGui::SameLine();
        if (playButton("##Stop", EditorIcon::Stop, false, !isEdit, "Stop"))
        {
            (void)engineContext.Stop();
            // The restored scene keeps its ids, but an entity made during Play is gone.
            if (m_SelectedEntity && !engineContext.GetECS().IsAlive(*m_SelectedEntity))
                m_SelectedEntity.reset();
        }

        // The collider wireframes' toggle and the settings gear sit at the right edge.
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - 2.0f * buttonSize - style.ItemSpacing.x);
        if (playButton("##Colliders", EditorIcon::Collider, m_Settings.PhysicsDebug, true,
                       m_Settings.PhysicsDebug ? "Hide colliders" : "Show colliders"))
            m_Settings.PhysicsDebug = !m_Settings.PhysicsDebug;

        ImGui::SameLine();
        if (IconButton("##Settings", GetIcon(EditorIcon::Settings), ICON_SIZE_SMALL, iconTint))
            m_SettingsWindowOpen = true;
        ImGui::SetItemTooltip("Settings");
    }

    // ─── Scene hierarchy ─────────────────────────────────────────────────────

    void EditorGui::DrawSceneHierarchy(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& sceneManager  = engineContext.GetSceneManager();
        auto& ecs           = engineContext.GetECS();

        // Header: "+" opens the create menu, the search field takes the rest of the row.
        const ImVec4 iconTint = ImGui::GetStyle().Colors[ImGuiCol_Text];
        if (IconButton("##HierarchyCreate", GetIcon(EditorIcon::Plus), ICON_SIZE_SMALL, iconTint))
            ImGui::OpenPopup("##HierarchyCreateMenu");
        ImGui::SetItemTooltip("Create");
        if (ImGui::BeginPopup("##HierarchyCreateMenu"))
        {
            if (ImGui::MenuItem("Create game object"))
                sceneManager.CreateGameObject(m_SelectedEntity);
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##HierarchySearch", "Search...", m_HierarchySearch, sizeof(m_HierarchySearch));

        const ECS::Entity root = sceneManager.GetRootEntity();
        const std::string_view search(m_HierarchySearch);
        const bool filtering = !search.empty();
        if (filtering)
            MarkHierarchyMatches(ecs, root, search);

        // A filtered tree keeps its open state apart, so clearing the search restores the tree.
        ImGui::PushID(filtering ? "filtered" : "tree");
        DrawHierarchyNode(context, root, filtering);
        ImGui::PopID();

        // Edits picked in a row's menu apply once the tree, which iterates the children, is drawn.
        if (m_HierarchyCreateUnder)
            sceneManager.CreateGameObject(*m_HierarchyCreateUnder);
        if (m_HierarchyDelete && ecs.IsAlive(*m_HierarchyDelete))
        {
            sceneManager.DeleteGameObject(*m_HierarchyDelete);
            if (m_SelectedEntity && !ecs.IsAlive(*m_SelectedEntity))
                m_SelectedEntity.reset();
        }
        if (m_HierarchyCreatePrefab && ecs.IsAlive(*m_HierarchyCreatePrefab))
            CreatePrefabFrom(context, *m_HierarchyCreatePrefab);
        m_HierarchyCreateUnder.reset();
        m_HierarchyDelete.reset();
        m_HierarchyCreatePrefab.reset();

        // The space under the tree takes drops too: a mesh dropped there goes under the root.
        const ImVec2 space = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##HierarchySpace", ImVec2(std::max(space.x, 1.0f), std::max(space.y, ImGui::GetFrameHeight())));
        if (const auto drop = AcceptAssetDrop({ ContentType::Mesh, ContentType::Prefab, ContentType::Scene }))
            m_AssetDrop = AssetDrop{ *drop, true, std::nullopt };
    }

    bool EditorGui::MarkHierarchyMatches(ECS::ECS& ecs, ECS::Entity entity, std::string_view search)
    {
        if (m_HierarchyMatches.size() != ECS::MAX_ENTITIES)
            m_HierarchyMatches.assign(ECS::MAX_ENTITIES, 0);

        const auto& component = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        bool shown = ContainsIgnoringCase(component.Name, search);
        for (const ECS::Entity child : component.Children)
            shown = MarkHierarchyMatches(ecs, child, search) || shown;
        m_HierarchyMatches[entity] = shown ? 1 : 0;
        return shown;
    }

    void EditorGui::DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, bool filtering)
    {
        auto&       engineContext = context.GetEngineContext();
        auto&       ecs           = engineContext.GetECS();
        const auto& component     = ecs.GetComponent<ECS::HierarchyComponent>(entity);
        const bool  isRoot        = entity == engineContext.GetSceneManager().GetRootEntity();

        // While searching, only matches and their ancestors show, the ancestors open.
        if (filtering && !isRoot && !m_HierarchyMatches[entity])
            return;

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen |
                                       ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (m_SelectedEntity == entity)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        const bool hasChildren = !component.Children.empty();
        if (!hasChildren)
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        else if (filtering)
            ImGui::SetNextItemOpen(true);

        const EditorIcon icon = isRoot ? EditorIcon::Scene : ChooseEntityIcon(MakeEntityTraits(ecs, entity));
        // A prefab instance's root shows its name and icon in the prefab colour.
        const bool   prefabRoot = ecs.HasComponent<HedgehogEngine::PrefabInstanceComponent>(entity) &&
                                ecs.GetComponent<HedgehogEngine::PrefabInstanceComponent>(entity).InstanceRoot == entity;
        const ImVec4 textColor  = prefabRoot ? Theme::Resolve(Theme::PREFAB_TINT) : ImGui::GetStyle().Colors[ImGuiCol_Text];

        // The label leaves room for the icon, drawn over it once the row is placed. The root row
        // shows the scene's name (a copy, so only the root row allocates).
        const std::string sceneName = isRoot ? engineContext.GetSceneManager().GetSceneName() : std::string();
        const char*       name      = isRoot ? sceneName.c_str() : component.Name.c_str();
        const float       labelX    = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        const bool        nodeOpen  = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity)),
                                                        nodeFlags, "%s%s", HIERARCHY_ICON_PAD, name);
        ImGui::PopStyleColor();
        const ImVec2      rowMin    = ImGui::GetItemRectMin();
        const float       rowH      = ImGui::GetItemRectSize().y;
        DrawIcon(*ImGui::GetWindowDrawList(), GetIcon(icon), ImVec2(labelX, rowMin.y + (rowH - ICON_SIZE_SMALL) * 0.5f),
                 ICON_SIZE_SMALL, ImGui::GetColorU32(textColor));

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_SelectedEntity = (m_SelectedEntity == entity) ? std::nullopt : std::optional<ECS::Entity>(entity);
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Create child"))
                m_HierarchyCreateUnder = entity;
            if (ImGui::MenuItem("Create Prefab...", nullptr, false, !isRoot))
                m_HierarchyCreatePrefab = entity;
            if (ImGui::MenuItem("Delete", nullptr, false, !isRoot))
                m_HierarchyDelete = entity;
            ImGui::EndPopup();
        }
        DragEntitySource(entity, component.Name);
        if (const auto drop = AcceptAssetDrop({ ContentType::Mesh, ContentType::Prefab, ContentType::Scene }))
            m_AssetDrop = AssetDrop{ *drop, true, entity };

        if (hasChildren && nodeOpen)
        {
            for (const ECS::Entity child : component.Children)
                DrawHierarchyNode(context, child, filtering);
            ImGui::TreePop();
        }
    }

    // ─── Inspector ───────────────────────────────────────────────────────────

    void EditorGui::DrawInspector(HedgehogEngine::Engine& context)
    {
        if (m_SelectedEntity.has_value())
        {
            RefreshPrefabOverrides(context);
            DrawEntityTitle(context);
            DrawPrefabBar(context);
            const bool instance = m_PrefabOverrides.InstanceRoot != ECS::INVALID_ENTITY;
            Reflection::ActivePrefabOverrideMarks() = instance ? &m_PrefabMarks : nullptr;
            // Every inspectable component the entity has, in SortOrder: a hand-drawn section when
            // one is keyed to the type, else the generic reflected section.
            auto& ecs = context.GetEngineContext().GetECS();
            for (const EcsSerialization::ComponentInfo* info : context.GetEngineContext().GetComponentTypes().GetInfosInOrder())
            {
                if (!info->Inspectable || !info->Has(ecs, *m_SelectedEntity))
                    continue;
                if (const auto drawer = m_InspectorDrawers.find(info->Key); drawer != m_InspectorDrawers.end())
                    (this->*drawer->second)(context);
                else
                    DrawRegisteredComponent(context, *info);
            }
            Reflection::ActivePrefabOverrideMarks() = nullptr;
            HandlePrefabRowRequest(context);
            // An edit in the inspector may have made or undone an override.
            if (instance && ImGui::IsAnyItemActive())
                m_PrefabOverrides.Stale = true;

            // The Component menu's items, from a button under the last section.
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f)))
                ImGui::OpenPopup("##AddComponent");
            if (ImGui::BeginPopup("##AddComponent"))
            {
                DrawAddComponentItems(context);
                ImGui::EndPopup();
            }
        }
        else
        {
            ImGui::TextDisabled("No entity selected.");
        }
    }

    void EditorGui::DrawEntityTitle(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto  entity        = m_SelectedEntity.value();
        auto& hierarchy     = ecs.GetComponent<ECS::HierarchyComponent>(entity);

        // The entity's icon, then its name filling the row.
        const float  rowH = ImGui::GetFrameHeight();
        const ImVec2 at   = ImGui::GetCursorScreenPos();
        DrawIcon(*ImGui::GetWindowDrawList(), GetIcon(ChooseEntityIcon(MakeEntityTraits(ecs, entity))),
                 ImVec2(at.x, at.y + (rowH - ICON_SIZE_SMALL) * 0.5f), ICON_SIZE_SMALL, ImGui::GetColorU32(ImGuiCol_Text));
        ImGui::Dummy(ImVec2(ICON_SIZE_SMALL, rowH));
        ImGui::SameLine();

        char nameBuf[256];
        strncpy_s(nameBuf, hierarchy.Name.c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
            hierarchy.Name = nameBuf;

        // The layer belongs to the entity's rendering; hand-drawn because layer names are edited at
        // runtime, so the combo reads them from settings every frame.
        if (ecs.HasComponent<HedgehogEngine::RenderComponent>(entity) && BeginPropertyTable("##entity"))
        {
            auto&       render = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
            const auto& layers = engineContext.GetSettings().GetLayerSettings();
            PropertyLabel("Layer");
            if (ImGui::BeginCombo("##Layer", layers->GetLayerDisplayName(render.Layer).c_str()))
            {
                for (uint32_t layer = 0; layer < HedgehogSettings::LayerSettings::LAYER_COUNT; ++layer)
                {
                    const bool isSelected = (render.Layer == layer);
                    if (ImGui::Selectable(layers->GetLayerDisplayName(layer).c_str(), isSelected))
                        render.Layer = layer;
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            EndPropertyTable();
        }
        ImGui::Spacing();
    }

    void EditorGui::DrawTransformComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto  entity        = m_SelectedEntity.value();

        ComponentHeaderOptions header;
        header.Icon      = GetIcon(EditorIcon::Transform);
        header.Removable = false;
        if (!ComponentHeader("Transform", header).Open)
            return;

        auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);

        if (Reflection::RenderComponentGui(&transform, HedgehogEngine::TransformComponent::GetProperties(), HedgehogEngine::TransformComponent::s_TypeName))
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
        const ComponentHeaderResult section = ComponentHeader("Mesh", { GetIcon(EditorIcon::Mesh), GetIcon(EditorIcon::More) });
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::MeshComponent>(entity);
            return;
        }
        if (!section.Open || !BeginPropertyTable("##Mesh"))
            return;

        auto&       mesh          = ecs.GetComponent<HedgehogEngine::MeshComponent>(entity);
        const auto& meshPaths     = meshSystem->GetMeshes();
        uint64_t    selectedIndex = mesh.MeshIndex.value_or(0);

        PropertyLabel("Mesh");
        const bool meshComboOpen = ImGui::BeginCombo("##mesh", mesh.MeshPath.c_str());
        AcceptSelectionDrop(ContentType::Mesh);
        if (meshComboOpen)
        {
            for (uint64_t i = 0; i < meshPaths.size(); ++i)
            {
                const bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(meshPaths[i].c_str(), isSelected))
                {
                    mesh.MeshPath = meshPaths[i];
                    meshSystem->Update(ecs, entity, engineContext.GetFileSystem());
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        PropertyLabel("");
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
        EndPropertyTable();
    }

    void EditorGui::DrawRenderComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* renderSystem  = engineContext.GetRenderSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::RenderComponent>(entity))
            return;

        // The header's checkbox is the component's visibility; the layer is in the entity header.
        auto&                  render = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
        ComponentHeaderOptions header{ GetIcon(EditorIcon::Material), GetIcon(EditorIcon::More), &render.IsVisible };
        const ComponentHeaderResult section = ComponentHeader("Rendering", header);
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::RenderComponent>(entity);
            return;
        }
        if (!section.Open || !BeginPropertyTable("##Rendering"))
            return;

        const auto& materials = renderSystem->GetMaterials();
        if (!materials.empty())
        {
            const uint64_t selectedIndex = render.MaterialIndex.value_or(0);
            PropertyLabel("Material");
            const bool materialComboOpen = ImGui::BeginCombo("##material", render.Material.c_str());
            AcceptSelectionDrop(ContentType::Material);
            if (materialComboOpen)
            {
                for (uint64_t i = 0; i < materials.size(); ++i)
                {
                    const bool isSelected = (selectedIndex == i);
                    if (ImGui::Selectable(materials[i].c_str(), isSelected))
                    {
                        render.Material = materials[i];
                        renderSystem->Update(ecs, entity);
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        PropertyLabel("");
        if (ImGui::Button("Load material"))
        {
            char* path = DialogueWindows::MaterialOpenDialogue();
            if (path != nullptr)
            {
                const auto virtualPath = engineContext.GetFileSystem().ToVirtualPath(path);
                if (virtualPath)
                {
                    // Strip "assets://" prefix; m_Material stores the relative path.
                    render.Material = virtualPath->substr(ASSETS_PREFIX.size());
                    renderSystem->Update(ecs, entity);
                }
                else
                {
                    LOGERROR("EditorGui: Load material path is not under any registered mount (path: ", path, ")");
                }
            }
        }

        if (!materials.empty() && render.MaterialIndex.has_value())
        {
            ImGui::SameLine();
            if (ImGui::Button("Save material"))
                engineContext.GetResourceCatalog().GetMaterialContainer().SaveMaterial(
                    render.MaterialIndex.value(), engineContext.GetFileSystem());

            DrawMaterialFields(engineContext.GetResourceCatalog().GetMaterialContainer(),
                               engineContext.GetResourceCatalog().GetTextureContainer(), render.MaterialIndex.value(),
                               engineContext.GetFileSystem());
        }
        EndPropertyTable();
    }

    void EditorGui::DrawLightComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* lightSystem   = engineContext.GetLightSystem();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::LightComponent>(entity))
            return;

        // The header's checkbox is the light's Enable (its reflected row is hidden).
        auto&                  light = ecs.GetComponent<HedgehogEngine::LightComponent>(entity);
        ComponentHeaderOptions header{ GetIcon(EditorIcon::Light), GetIcon(EditorIcon::More), &light.Enable };
        const ComponentHeaderResult section = ComponentHeader("Light", header);
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::LightComponent>(entity);
            return;
        }
        if (!section.Open)
            return;

        Reflection::RenderComponentGui(&light, HedgehogEngine::LightComponent::GetProperties(), HedgehogEngine::LightComponent::s_TypeName);

        // Through the light system, so only one light casts shadows.
        if (light.LightType == HedgehogEngine::LightType::DirectionLight && BeginPropertyTable("##LightShadows"))
        {
            bool castShadows = light.CastShadows;
            PropertyLabel("Cast shadows");
            if (ImGui::Checkbox("##CastShadows", &castShadows))
                lightSystem->SetShadowCasting(ecs, entity, castShadows);
            EndPropertyTable();
        }
    }

    void EditorGui::DrawCameraComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::CameraComponent>(entity))
            return;

        // The header's checkbox is the camera's IsEnabled (its reflected row is hidden).
        auto&                  camera = ecs.GetComponent<HedgehogEngine::CameraComponent>(entity);
        ComponentHeaderOptions header{ GetIcon(EditorIcon::Camera), GetIcon(EditorIcon::More), &camera.IsEnabled };
        const ComponentHeaderResult section = ComponentHeader("Camera", header);
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::CameraComponent>(entity);
            return;
        }
        if (!section.Open)
            return;

        Reflection::RenderComponentGui(&camera, HedgehogEngine::CameraComponent::GetProperties(), HedgehogEngine::CameraComponent::s_TypeName);
        if (!BeginPropertyTable("##CameraMore"))
            return;
        DrawCameraGraph(camera.GraphName);

        // Hand-drawn, like the entity header's Layer combo: named checkboxes need live layer names
        // from settings, which the reflected uint32 widget has no way to source.
        PropertyLabel("Layers");
        const auto& layers = engineContext.GetSettings().GetLayerSettings();
        for (uint32_t layer = 0; layer < HedgehogSettings::LayerSettings::LAYER_COUNT; ++layer)
        {
            const uint32_t bit     = 1u << layer;
            bool           checked = (camera.LayerMask & bit) != 0;

            ImGui::PushID(static_cast<int>(layer));
            if (ImGui::Checkbox(layers->GetLayerDisplayName(layer).c_str(), &checked))
            {
                camera.LayerMask = checked ? (camera.LayerMask | bit) : (camera.LayerMask & ~bit);
            }
            ImGui::PopID();

            if (layer % 2 != 1)
                ImGui::SameLine();
        }
        EndPropertyTable();
    }

    void EditorGui::DrawAnimatorComponent(HedgehogEngine::Engine& context)
    {
        auto& ecs    = context.GetEngineContext().GetECS();
        auto  entity = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::AnimatorComponent>(entity))
            return;
        const ComponentHeaderResult section = ComponentHeader("Animator", { GetIcon(EditorIcon::Animator), GetIcon(EditorIcon::More) });
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::AnimatorComponent>(entity);
            return;
        }
        if (!section.Open)
            return;

        auto& animator = ecs.GetComponent<HedgehogEngine::AnimatorComponent>(entity);
        Reflection::RenderComponentGui(&animator, HedgehogEngine::AnimatorComponent::GetProperties(), HedgehogEngine::AnimatorComponent::s_TypeName);
    }

    // A registered component's reflected rows under its icon header (its enabled property as the
    // header's checkbox), then any extra rows keyed to it; the header's menu removes it.
    void EditorGui::DrawRegisteredComponent(HedgehogEngine::Engine& context, const EcsSerialization::ComponentInfo& info)
    {
        auto&             ecs       = context.GetEngineContext().GetECS();
        const ECS::Entity entity    = m_SelectedEntity.value();
        void*             component = info.Get(ecs, entity);

        // The enabled property is the header's checkbox, so its own row draws nothing (it still
        // serialises). Set once per type, when first drawn.
        if (!info.EnabledProperty.empty() && m_HiddenEnabledRows.insert(info.Key).second)
        {
            for (const Reflection::PropertyDescriptor& prop : info.Properties)
            {
                // The descriptors live in the component's mutable property table.
                if (info.EnabledProperty == prop.name)
                    const_cast<Reflection::PropertyDescriptor&>(prop).guiOverride =
                        [](void*, const Reflection::PropertyDescriptor&) -> bool { return false; };
            }
        }

        const std::optional<EditorIcon> icon = FindEditorIcon(info.Icon);
        ComponentHeaderOptions          header{ icon ? GetIcon(*icon) : nullptr, GetIcon(EditorIcon::More), info.Enabled(component) };
        header.Removable                    = info.Removable;
        const ComponentHeaderResult section = ComponentHeader(info.DisplayName.c_str(), header);
        if (section.RemoveRequested)
        {
            info.Remove(ecs, entity);
            return;
        }
        if (!section.Open)
            return;
        // The component's asset field is also where a file of its type is dropped.
        Reflection::PropertyWidgetHook assetDrop;
        if (const auto drop = m_InspectorAssetDrops.find(info.Key); drop != m_InspectorAssetDrops.end())
        {
            assetDrop = [this, target = drop->second](const Reflection::PropertyDescriptor& prop)
            {
                if (std::string_view(prop.name) != target.Property)
                    return;
                ImGui::SetItemTooltip("Drop %s here from the Project panel.", target.Hint);
                AcceptSelectionDrop(target.Type);
            };
        }
        ImGui::PushID(info.Key.c_str());
        Reflection::RenderComponentGui(component, info.Properties, info.Key.c_str(), &assetDrop);
        ImGui::PopID();
    }

    // Hand-drawn instead of the reflected text field, so a camera picks from the graphs that exist,
    // or any .graph file with Browse..., and Edit opens its graph in the render graph editor. A
    // reference with no usable graph (an unknown name, a missing or broken file) shows in red with
    // a tooltip: the camera's view is skipped until another graph is picked.
    void EditorGui::DrawCameraGraph(std::string& graphName)
    {
        constexpr ImVec4 MISSING_GRAPH_COLOR = { 0.95f, 0.35f, 0.35f, 1.0f };
        if (!m_Renderer)
            return;

        // An engine graph by its name, a file by its file name; the drop-down's tooltip gives the path.
        const auto label = [](const std::string& reference)
        {
            if (Renderer::ClassifyGraphReference(reference) == Renderer::GraphReferenceKind::Name)
                return reference;
            return std::filesystem::path(reference).filename().string();
        };
        const auto isEditorGraph = [](std::string_view name) { return name == SCENE_GRAPH || name == RESULT_GRAPH; };
        const bool usable        = m_Renderer->FindGraphAsset(graphName) != nullptr;

        PropertyLabel("Graph");
        if (!usable)
            ImGui::PushStyleColor(ImGuiCol_Text, MISSING_GRAPH_COLOR);
        const bool open = ImGui::BeginCombo("##GraphName", graphName.empty() ? "(none)" : label(graphName).c_str());
        AcceptSelectionDrop(ContentType::RenderGraph);
        if (!usable)
            ImGui::PopStyleColor();
        if (ImGui::IsItemHovered() && !graphName.empty())
        {
            // A file no view has asked for yet (its view is hidden, say) has no error recorded:
            // trying it now, only while hovered, says whether it is missing or broken.
            if (!usable && Renderer::ClassifyGraphReference(graphName) == Renderer::GraphReferenceKind::File)
                (void)m_Renderer->LoadGraph(graphName);
            const std::string_view error = m_Renderer->GetGraphError(graphName);
            if (usable)
                ImGui::SetTooltip("%s", graphName.c_str());
            else if (!error.empty())
                ImGui::SetTooltip("%s\n%s\nThis camera's view is skipped.", graphName.c_str(), std::string(error).c_str());
            else
                ImGui::SetTooltip("No graph asset named '%s': this camera's view is skipped.", graphName.c_str());
        }
        if (open)
        {
            // Graphs a scene camera would pick first, then the editor's own views' graphs.
            for (const bool editorGraphs : { false, true })
            {
                bool separated = !editorGraphs;
                for (const std::string& name : m_Renderer->GetGraphNames())
                {
                    if (isEditorGraph(name) != editorGraphs)
                        continue;
                    if (!separated)
                    {
                        ImGui::Separator();
                        separated = true;
                    }
                    const std::string shown  = editorGraphs ? name + " (editor)" : label(name);
                    const bool        chosen = name == graphName;
                    ImGui::PushID(name.c_str());
                    if (ImGui::Selectable(shown.c_str(), chosen))
                        graphName = name;
                    if (ImGui::IsItemHovered() && shown != name)
                        ImGui::SetTooltip("%s", name.c_str());
                    ImGui::PopID();
                    if (chosen)
                        ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // On their own row: the inspector is narrow, and the drop-down lines up with the fields above.
        PropertyLabel("");
        if (ImGui::Button("Browse..."))
        {
            const char* picked = DialogueWindows::RenderGraphOpenDialogue(GetGraphDialoguePath(*m_FileSystem, "").c_str());
            if (picked)
            {
                graphName = MakeGraphReference(picked, *m_Renderer, *m_FileSystem);
                (void)m_Renderer->LoadGraph(graphName); // one that fails to load shows in red, with why
            }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(graphName.empty());
        if (ImGui::Button("Edit"))
        {
            (void)m_Renderer->LoadGraph(graphName);
            m_RenderGraphEditorWindow->OpenGraph(graphName);
        }
        ImGui::EndDisabled();
    }

    void EditorGui::DrawScriptComponent(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto  entity        = m_SelectedEntity.value();

        if (!ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
            return;

        // The header's checkbox is the script's Enable.
        auto&                  component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);
        ComponentHeaderOptions header{ GetIcon(EditorIcon::Script), GetIcon(EditorIcon::More), &component.Enable };
        const ComponentHeaderResult section = ComponentHeader("Script", header);
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<HedgehogEngine::ScriptComponent>(entity);
            return;
        }
        if (!section.Open)
            return;

        if (BeginPropertyTable("##Script"))
        {
            const std::string& scriptSrc = component.ScriptPath.empty()
                ? "No script selected" : component.ScriptPath;
            char scriptBuf[256];
            strncpy_s(scriptBuf, scriptSrc.c_str(), sizeof(scriptBuf) - 1);
            scriptBuf[sizeof(scriptBuf) - 1] = '\0';
            PropertyLabel("Script");
            ImGui::InputText("##Script", scriptBuf, sizeof(scriptBuf));
            AcceptSelectionDrop(ContentType::Script);

            PropertyLabel("");
            ImGui::BeginDisabled(engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit);
            if (ImGui::Button("Load script"))
            {
                std::string scriptPath = DialogueWindows::ScriptChooseDialogue();
                if (!scriptPath.empty())
                    (void)AssignScript(context, entity, scriptPath);
            }
            ImGui::EndDisabled();
            EndPropertyTable();
        }

        // A property edit reaches the running script at once; outside Play it only changes the
        // data, and Stop's restore puts back whatever was edited during Play.
        const auto* declarations = FindScriptDeclarations(engineContext.GetFileSystem(), component.ScriptPath);
        if (DrawScriptProperties(component, declarations, ecs) && m_ScriptSystem)
            m_ScriptSystem->PushProperties(ecs, entity);
    }

    const std::vector<HedgehogScripting::ScriptPropertyDeclaration>* EditorGui::FindScriptDeclarations(
        const FS::FileSystemManager& fileSystem, const std::string& scriptPath)
    {
        if (!m_ScriptSystem || scriptPath.empty())
            return nullptr;
        const std::string virtualPath = HedgehogScripting::ScriptSystem::NormalizeScriptPath(scriptPath);
        const auto        physical    = fileSystem.ResolvePhysical(virtualPath);
        if (!physical)
            return nullptr;
        std::error_code error;
        const auto      writeTime = std::filesystem::last_write_time(*physical, error);
        if (error)
            return nullptr;

        const auto cached = m_ScriptDeclarations.find(virtualPath);
        if (cached != m_ScriptDeclarations.end() && cached->second.WriteTime == writeTime)
            return &cached->second.Declarations;
        ScriptDeclarations& described = m_ScriptDeclarations[virtualPath];
        described = ScriptDeclarations{ writeTime, m_ScriptSystem->DescribeScript(virtualPath) };
        return &described.Declarations;
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
            if (ImGui::Button("Save settings"))
                m_Settings.Save(EditorSettings::PATH,
                                context.GetEngineContext().GetFileSystem());
            ImGui::SameLine();
            if (ImGui::Button("Load settings"))
                m_Settings.Load(EditorSettings::PATH,
                                context.GetEngineContext().GetFileSystem());
        }

        auto& engineContext = context.GetEngineContext();
        auto& settings      = engineContext.GetSettings();

        ImGui::SeparatorText("Engine settings");
        if (ImGui::Button("Save engine settings"))
        {
            if (!settings.Save(HedgehogSettings::Settings::PATH, engineContext.GetFileSystem()))
            {
                LOGWARNING("EditorGui: failed to save engine settings.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload engine settings"))
        {
            if (!settings.Load(HedgehogSettings::Settings::PATH, engineContext.GetFileSystem()))
            {
                LOGWARNING("EditorGui: no engine settings file to reload.");
            }
        }

        if (ImGui::CollapsingHeader("Layers"))
        {
            ImGui::TextWrapped(
                "A scene stores a layer's index, never its name. Renaming a layer relabels it "
                "and never moves an object between layers.");
            ImGui::Spacing();

            auto& layers = settings.GetLayerSettings();
            for (uint32_t layer = 0; layer < HedgehogSettings::LayerSettings::LAYER_COUNT; ++layer)
            {
                const std::string& name = layers->GetLayerName(layer);

                char buffer[64];
                buffer[name.copy(buffer, sizeof(buffer) - 1)] = '\0';

                const std::string label = "Layer " + std::to_string(layer);

                ImGui::PushID(static_cast<int>(layer));
                if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
                {
                    // Blanking layer 0 is refused by SetLayerName; the field repopulates with
                    // "Default" on the next frame.
                    layers->SetLayerName(layer, buffer);
                }
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Physics"))
        {
            ImGui::TextWrapped("Applied when Play starts. Bodies store a physics layer's index, never its name.");
            DrawPhysicsSettingsFields(settings.GetPhysicsSettings());
        }

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

            // Read by the forward pass when it samples the atlas; saved with the engine settings.
            ImGui::SeparatorText("Sampling");
            float depthBias = shadow->GetDepthBias();
            if (ImGui::DragFloat("Depth bias", &depthBias, 0.0001f, 0.0f, HedgehogSettings::ShadowmapSettings::MAX_DEPTH_BIAS,
                                 "%.5f", ImGuiSliderFlags_AlwaysClamp))
                shadow->SetDepthBias(depthBias);
            float slopeBias = shadow->GetSlopeBias();
            if (ImGui::DragFloat("Slope bias", &slopeBias, 0.0005f, 0.0f, HedgehogSettings::ShadowmapSettings::MAX_SLOPE_BIAS,
                                 "%.4f", ImGuiSliderFlags_AlwaysClamp))
                shadow->SetSlopeBias(slopeBias);
            float normalOffset = shadow->GetNormalOffset();
            if (ImGui::SliderFloat("Normal offset (texels)", &normalOffset, 0.0f,
                                   HedgehogSettings::ShadowmapSettings::MAX_NORMAL_OFFSET))
                shadow->SetNormalOffset(normalOffset);
            int pcfRadius = static_cast<int>(shadow->GetPcfRadius());
            if (ImGui::SliderInt("PCF radius", &pcfRadius, 0, static_cast<int>(HedgehogSettings::ShadowmapSettings::MAX_PCF_RADIUS)))
                shadow->SetPcfRadius(static_cast<uint32_t>(pcfRadius));
            float cascadeBlend = shadow->GetCascadeBlend();
            if (ImGui::SliderFloat("Cascade blend", &cascadeBlend, 0.0f, HedgehogSettings::ShadowmapSettings::MAX_CASCADE_BLEND))
                shadow->SetCascadeBlend(cascadeBlend);

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

    // ─── Last-scene persistence (per project, in its recent-projects entry) ──

    std::optional<std::filesystem::path> EditorGui::TakeProjectRequest()
    {
        return std::exchange(m_ProjectRequest, std::nullopt);
    }

    void EditorGui::DrawProjectMenuItems(bool editing)
    {
        // Switching projects rebuilds the editor, which a Play session cannot survive.
        if (ImGui::MenuItem("New Project...", nullptr, false, editing))
            m_NewProjectWindow->Show(m_ProjectRoot);
        if (ImGui::MenuItem("Open Project...", nullptr, false, editing))
        {
            const std::string start = m_ProjectRoot.parent_path().string();
            if (const char* folder = DialogueWindows::ProjectOpenDialogue(start.c_str()))
                RequestProject(folder);
        }
        if (!ImGui::BeginMenu("Recent Projects", editing))
            return;

        std::optional<std::filesystem::path> chosen;
        bool                                 anyMissing = false;
        for (const RecentProject& project : m_Settings.RecentProjects)
        {
            const bool        open    = IsSameProject(project.Path, m_ProjectRoot);
            const bool        missing = !IsProjectFolder(project.Path);
            const std::string path    = project.Path.string();
            const std::string label   = project.Path.filename().string() + (missing ? " (missing)" : "") + "##" + path;
            anyMissing |= missing;
            if (ImGui::MenuItem(label.c_str(), path.c_str(), open, !open && !missing))
                chosen = project.Path;
        }
        if (m_Settings.RecentProjects.empty())
            ImGui::MenuItem("No recent projects", nullptr, false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Remove Missing Projects", nullptr, false, anyMissing))
        {
            (void)RemoveMissingProjects(m_Settings.RecentProjects);
            m_Settings.Save(EditorSettings::PATH, *m_FileSystem);
        }
        ImGui::EndMenu();

        // After the loop: a request reorders the list it walks.
        if (chosen)
            RequestProject(*chosen);
    }

    void EditorGui::RequestProject(const std::filesystem::path& folder, bool confirm)
    {
        const std::filesystem::path project = NormalizeProjectPath(folder);
        if (IsSameProject(project, m_ProjectRoot))
        {
            LOGINFO("[Editor] ", project.string(), " is the open project already.");
            return;
        }

        // Read through a project:// of its own, as the engine will once rebuilt on it.
        HedgehogSettings::ProjectSettings settings;
        FS::FileSystemManager             files;
        auto                              mount = std::make_unique<FS::FileSystem>();
        const bool readable = IsProjectFolder(project) && mount->RegisterPath(FS::PROJECT_ALIAS, project) &&
                              files.Register(std::move(mount)) &&
                              settings.Load(HedgehogSettings::ProjectSettings::PATH, files);
        if (!readable)
        {
            LOGERROR("[Editor] Cannot open ", project.string(), ": it holds no readable Project.yaml.");
            return;
        }
        if (confirm && !DialogueWindows::ConfirmProjectSwitch(settings.GetName().c_str()))
            return;

        // Saved with the user settings as the editor closes, so the restarted editor (and the next
        // run) finds it first.
        (void)TouchRecentProject(m_Settings.RecentProjects, project);
        m_ProjectRequest = project;
        LOGINFO("[Editor] Switching to the project at ", project.string(), ".");
    }

    RecentProject& EditorGui::CurrentProject()
    {
        if (RecentProject* project = FindRecentProject(m_Settings.RecentProjects, m_ProjectRoot))
            return *project;
        if (m_RecordRecentProject)
            return TouchRecentProject(m_Settings.RecentProjects, m_ProjectRoot);
        m_UnrecordedProject.Path = m_ProjectRoot;
        return m_UnrecordedProject;
    }

    void EditorGui::RecordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem)
    {
        const auto virtualPath = fileSystem.ToVirtualPath(nativePath);
        if (!virtualPath)
        {
            LOGERROR("EditorGui::RecordLastScene: path is not under any registered mount (path: ", nativePath, ")");
            return;
        }

        CurrentProject().LastScene = *virtualPath;
        m_Settings.Save(EditorSettings::PATH, fileSystem);
    }

    void EditorGui::LoadLastScene(HedgehogEngine::Engine& context)
    {
        // A project never opened here (or whose last scene was cleared) starts on its startup scene.
        if (CurrentProject().LastScene.empty())
            CurrentProject().LastScene = context.GetEngineContext().GetSettings().GetProjectSettings().GetStartupScene();
        if (CurrentProject().LastScene.empty())
            return;

        if (!m_FileSystem->Exists(CurrentProject().LastScene))
        {
            LOGWARNING("EditorGui::LoadLastScene: stored scene not found, starting with an empty scene (path: ",
                       CurrentProject().LastScene, ")");
            CurrentProject().LastScene.clear();
            m_Settings.Save(EditorSettings::PATH, *m_FileSystem);
            return;
        }

        // LoadScene expects a native path (the dialog's contract), so resolve back from virtual.
        const auto physicalPath = m_FileSystem->ResolvePhysical(CurrentProject().LastScene);
        if (physicalPath && context.GetEngineContext().GetSceneManager().LoadScene(physicalPath->string()))
            return;

        LOGWARNING("EditorGui::LoadLastScene: failed to load stored scene, starting with an empty scene (path: ",
                   CurrentProject().LastScene, ")");
        context.GetEngineContext().GetSceneManager().ResetScene();
        CurrentProject().LastScene.clear();
        m_Settings.Save(EditorSettings::PATH, *m_FileSystem);
    }
}
