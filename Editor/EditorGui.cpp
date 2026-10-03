#include "EditorGui.hpp"
#include "EditorTheme.hpp"
#include "Widgets/IconWidgets.hpp"
#include "Widgets/PropertyFields.hpp"
#include "Widgets/ViewportOverlay.hpp"
#include "Panels/EntityIcon.hpp"
#include "Panels/TextSearch.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Panels/ContentPanel.hpp"
#include "Tools/VertexDescriptionWindow.hpp"
#include "Tools/PipelineWindow.hpp"
#include "Tools/ShaderWindow.hpp"
#include "Tools/InputActionsWindow.hpp"
#include "Panels/AssetDragDrop.hpp"
#include "Panels/EntityDragDrop.hpp"
#include "Panels/ScriptPropertyFields.hpp"
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
#include "HedgehogEngine/HedgehogSettings/api/ShadowmapingSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
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

namespace
{
    constexpr std::string_view ASSETS_PREFIX = "assets://";

    // An Add Component menu item that adds a default T to the selected entity, when it has none.
    template<typename T>
    void AddComponentMenuItem(ECS::ECS& ecs, const std::optional<ECS::Entity>& selected, const char* label)
    {
        if (ImGui::MenuItem(label) && selected.has_value() && !ecs.HasComponent<T>(*selected))
            ecs.AddComponent(*selected, T{});
    }

    // A component's reflected fields under an icon header, then whatever extra draws; the header's
    // menu removes it.
    template<typename T, typename Extra = void (*)()>
    void DrawReflectedComponent(ECS::ECS& ecs, ECS::Entity entity, const char* header, void* icon, void* menuIcon,
                                Extra extra = [] {})
    {
        if (!ecs.HasComponent<T>(entity))
            return;
        const Editor::ComponentHeaderResult section = Editor::ComponentHeader(header, { icon, menuIcon });
        if (section.RemoveRequested)
        {
            ecs.RemoveComponent<T>(entity);
            return;
        }
        if (!section.Open)
            return;
        ImGui::PushID(header);
        Reflection::RenderComponentGui(&ecs.GetComponent<T>(entity), T::GetProperties());
        extra();
        ImGui::PopID();
    }

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

    EditorGui::EditorGui(HedgehogEngine::Engine& context)
        : m_ConsolePanel(std::make_unique<ConsolePanel>())
        , m_VertexDescWindow(std::make_unique<VertexDescriptionWindow>())
        , m_PipelineWindow(std::make_unique<PipelineWindow>())
        , m_ShaderWindow(std::make_unique<ShaderWindow>())
        , m_InputActionsWindow(std::make_unique<InputActionsWindow>())
        , m_RenderGraphEditorWindow(std::make_unique<RenderGraphEditorWindow>())
    {
        m_FileSystem   = &context.GetEngineContext().GetFileSystem();
        m_ContentPanel = std::make_unique<ContentPanel>(*m_FileSystem);
        if (m_Settings.Load("engine://editor_settings.yaml", *m_FileSystem)
            && m_Settings.dockLayout.IsValid())
            m_DockSystem.GetLayout() = m_Settings.dockLayout;


        SetupLightComponentGuiOverrides();
        SetupCameraComponentGuiOverrides();

        LoadLastScene(context);
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

    void EditorGui::Draw(HedgehogEngine::Engine& context, const ViewportImages& images)
    {
        m_SceneViewHovered = false;
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
        else if (const std::optional<ContentOpenRequest> request = m_ContentPanel->Draw(m_ViewportImages.ContentIcons))
            OpenContentItem(context, *request);
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

                // A click, not the end of a camera drag.
                constexpr float CLICK_DRAG_PIXELS = 4.0f;
                const ImGuiIO&  io = ImGui::GetIO();
                if (m_SceneViewHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)
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
        region.PointerEnabled  = m_SceneViewHovered;
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

    void EditorGui::DrawAddComponentItems(HedgehogEngine::Engine& context)
    {
        auto& engineContext = context.GetEngineContext();
        auto& ecs           = engineContext.GetECS();
        auto* meshSystem    = engineContext.GetMeshSystem();
        auto* renderSystem  = engineContext.GetRenderSystem();

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
        if (ImGui::MenuItem("Camera component") && m_SelectedEntity.has_value())
        {
            ECS::Entity e = m_SelectedEntity.value();
            if (!ecs.HasComponent<HedgehogEngine::CameraComponent>(e))
                ecs.AddComponent(e, HedgehogEngine::CameraComponent{});
        }
        if (ImGui::MenuItem("Script component") && m_SelectedEntity.has_value())
        {
            ECS::Entity e = m_SelectedEntity.value();
            if (!ecs.HasComponent<HedgehogEngine::ScriptComponent>(e))
                ecs.AddComponent(e, HedgehogEngine::ScriptComponent{});
        }
        if (ImGui::MenuItem("Animator component") && m_SelectedEntity.has_value())
        {
            ECS::Entity e = m_SelectedEntity.value();
            if (!ecs.HasComponent<HedgehogEngine::AnimatorComponent>(e))
                ecs.AddComponent(e, HedgehogEngine::AnimatorComponent{});
        }
        ImGui::Separator();
        AddComponentMenuItem<HedgehogEngine::UiCanvasComponent>(ecs, m_SelectedEntity, "UI canvas component");
        AddComponentMenuItem<HedgehogEngine::UiRectComponent>(ecs, m_SelectedEntity, "UI rect component");
        AddComponentMenuItem<HedgehogEngine::UiImageComponent>(ecs, m_SelectedEntity, "UI image component");
        AddComponentMenuItem<HedgehogEngine::UiTextComponent>(ecs, m_SelectedEntity, "UI text component");
        AddComponentMenuItem<HedgehogEngine::UiButtonComponent>(ecs, m_SelectedEntity, "UI button component");
        ImGui::Separator();
        AddComponentMenuItem<HedgehogEngine::AudioSourceComponent>(ecs, m_SelectedEntity, "Audio source component");
        AddComponentMenuItem<HedgehogEngine::AudioListenerComponent>(ecs, m_SelectedEntity, "Audio listener component");
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

        // The settings gear sits at the right edge.
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - buttonSize);
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
        m_HierarchyCreateUnder.reset();
        m_HierarchyDelete.reset();

        // The space under the tree takes drops too: a mesh dropped there goes under the root.
        const ImVec2 space = ImGui::GetContentRegionAvail();
        ImGui::InvisibleButton("##HierarchySpace", ImVec2(std::max(space.x, 1.0f), std::max(space.y, ImGui::GetFrameHeight())));
        if (const auto drop = AcceptAssetDrop({ ContentType::Mesh, ContentType::Scene }))
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

        // The label leaves room for the icon, drawn over it once the row is placed. The root row
        // shows the scene's name (a copy, so only the root row allocates).
        const std::string sceneName = isRoot ? engineContext.GetSceneManager().GetSceneName() : std::string();
        const char*       name      = isRoot ? sceneName.c_str() : component.Name.c_str();
        const float       labelX    = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
        const bool        nodeOpen  = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entity)),
                                                        nodeFlags, "%s%s", HIERARCHY_ICON_PAD, name);
        const ImVec2      rowMin    = ImGui::GetItemRectMin();
        const float       rowH      = ImGui::GetItemRectSize().y;
        DrawIcon(*ImGui::GetWindowDrawList(), GetIcon(icon), ImVec2(labelX, rowMin.y + (rowH - ICON_SIZE_SMALL) * 0.5f),
                 ICON_SIZE_SMALL, ImGui::GetColorU32(ImGuiCol_Text));

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_SelectedEntity = (m_SelectedEntity == entity) ? std::nullopt : std::optional<ECS::Entity>(entity);
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Create child"))
                m_HierarchyCreateUnder = entity;
            if (ImGui::MenuItem("Delete", nullptr, false, !isRoot))
                m_HierarchyDelete = entity;
            ImGui::EndPopup();
        }
        DragEntitySource(entity, component.Name);
        if (const auto drop = AcceptAssetDrop({ ContentType::Mesh, ContentType::Scene }))
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
            DrawEntityTitle(context);
            DrawTransformComponent(context);
            DrawLightComponent(context);
            DrawCameraComponent(context);
            DrawMeshComponent(context);
            DrawRenderComponent(context);
            DrawScriptComponent(context);
            DrawAnimatorComponent(context);
            DrawUiComponents(context);
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

            auto& materialContainer = engineContext.GetResourceCatalog().GetMaterialContainer();
            auto& textureContainer  = engineContext.GetResourceCatalog().GetTextureContainer();
            auto& materialData      = materialContainer.GetMaterialDataByIndex(
                render.MaterialIndex.value());

            const char* typeNames[] = { "Opaque", "Cutoff", "Transparent" };
            int materialType = static_cast<int>(materialData.type);
            PropertyLabel("Type");
            if (ImGui::Combo("##Type", &materialType, typeNames, IM_ARRAYSIZE(typeNames)))
                materialData.type = static_cast<HedgehogEngine::MaterialType>(materialType);

            const auto& texturePaths = textureContainer.GetTexturePathes();
            int selectedTexture      = static_cast<int>(
                textureContainer.GetTextureIndex(materialData.baseColor));

            PropertyLabel("Base colour");
            const bool textureComboOpen = ImGui::BeginCombo("##baseColor", materialData.baseColor.c_str());
            AcceptSelectionDrop(ContentType::Texture);
            if (textureComboOpen)
            {
                for (int i = 0; i < static_cast<int>(texturePaths.size()); ++i)
                {
                    const bool isSelected = (selectedTexture == i);
                    if (ImGui::Selectable(texturePaths[i].c_str(), isSelected))
                    {
                        materialData.baseColor = texturePaths[i];
                        materialContainer.SetMaterialDirty(render.MaterialIndex.value());
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            PropertyLabel("");
            if (ImGui::Button("Load texture"))
            {
                if (const char* texPath = DialogueWindows::TextureOpenDialogue())
                {
                    const auto& fs = engineContext.GetFileSystem();
                    const auto virtualPath = fs.ToVirtualPath(texPath);
                    if (virtualPath)
                    {
                        materialContainer.LoadBaseTexture(render.MaterialIndex.value(),
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
                PropertyLabel("Transparency");
                if (ImGui::SliderFloat("##Transparency", &transparency, 0.0f, 1.0f))
                {
                    if (materialData.transparency != transparency)
                    {
                        materialData.transparency = transparency;
                        materialContainer.SetMaterialDirty(render.MaterialIndex.value());
                    }
                }
            }
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

        Reflection::RenderComponentGui(&light, HedgehogEngine::LightComponent::GetProperties());

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

        Reflection::RenderComponentGui(&camera, HedgehogEngine::CameraComponent::GetProperties());
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
        if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& animator = ecs.GetComponent<HedgehogEngine::AnimatorComponent>(entity);
        Reflection::RenderComponentGui(&animator, HedgehogEngine::AnimatorComponent::GetProperties());

        if (ImGui::Button("Remove animator"))
            ecs.RemoveComponent<HedgehogEngine::AnimatorComponent>(entity);
    }

    void EditorGui::DrawUiComponents(HedgehogEngine::Engine& context)
    {
        auto&             ecs    = context.GetEngineContext().GetECS();
        const ECS::Entity entity = m_SelectedEntity.value();
        void*             menu   = GetIcon(EditorIcon::More);
        DrawReflectedComponent<HedgehogEngine::UiCanvasComponent>(ecs, entity, "UI canvas", GetIcon(EditorIcon::UiCanvas), menu);
        DrawReflectedComponent<HedgehogEngine::UiRectComponent>(ecs, entity, "UI rect", GetIcon(EditorIcon::UiRect), menu);
        DrawReflectedComponent<HedgehogEngine::UiImageComponent>(ecs, entity, "UI image", GetIcon(EditorIcon::UiImage), menu);
        DrawReflectedComponent<HedgehogEngine::UiTextComponent>(ecs, entity, "UI text", GetIcon(EditorIcon::UiText), menu, [&]
        {
            ImGui::TextDisabled("Drop a font (.ttf, .otf) here");
            AcceptSelectionDrop(ContentType::Font);
        });
        DrawReflectedComponent<HedgehogEngine::UiButtonComponent>(ecs, entity, "UI button", GetIcon(EditorIcon::UiButton), menu);
        DrawReflectedComponent<HedgehogEngine::AudioSourceComponent>(ecs, entity, "Audio source", GetIcon(EditorIcon::AudioSource), menu, [&]
        {
            ImGui::TextDisabled("Drop an audio clip (.wav, .mp3, .flac) here");
            AcceptSelectionDrop(ContentType::Audio);
        });
        DrawReflectedComponent<HedgehogEngine::AudioListenerComponent>(ecs, entity, "Audio listener", GetIcon(EditorIcon::AudioListener), menu);
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
        if (!ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
            return;

        auto& component = ecs.GetComponent<HedgehogEngine::ScriptComponent>(entity);

        bool enabled = component.Enable;
        if (ImGui::Checkbox("Enabled", &enabled))
            component.Enable = enabled;

        {
            const std::string& scriptSrc = component.ScriptPath.empty()
                ? "No script selected" : component.ScriptPath;
            char scriptBuf[256];
            strncpy_s(scriptBuf, scriptSrc.c_str(), sizeof(scriptBuf) - 1);
            scriptBuf[sizeof(scriptBuf) - 1] = '\0';
            ImGui::InputText("Script", scriptBuf, sizeof(scriptBuf));
            AcceptSelectionDrop(ContentType::Script);
        }

        ImGui::BeginDisabled(engineContext.GetPlayState() != HedgehogEngine::PlayState::Edit);
        if (ImGui::Button("Load script"))
        {
            std::string scriptPath = DialogueWindows::ScriptChooseDialogue();
            if (!scriptPath.empty())
                (void)AssignScript(context, entity, scriptPath);
        }
        ImGui::EndDisabled();

        // A property edit reaches the running script at once; outside Play it only changes the
        // data, and Stop's restore puts back whatever was edited during Play.
        const auto* declarations = FindScriptDeclarations(engineContext.GetFileSystem(), component.ScriptPath);
        if (DrawScriptProperties(component, declarations, ecs) && m_ScriptSystem)
            m_ScriptSystem->PushProperties(ecs, entity);

        if (ImGui::Button("Remove script"))
        {
            if (ecs.HasComponent<HedgehogEngine::ScriptComponent>(entity))
                ecs.RemoveComponent<HedgehogEngine::ScriptComponent>(entity);
        }
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
                m_Settings.Save("engine://editor_settings.yaml",
                                context.GetEngineContext().GetFileSystem());
            ImGui::SameLine();
            if (ImGui::Button("Load settings"))
                m_Settings.Load("engine://editor_settings.yaml",
                                context.GetEngineContext().GetFileSystem());
        }

        auto& engineContext = context.GetEngineContext();
        auto& settings      = engineContext.GetSettings();

        ImGui::SeparatorText("Engine settings");
        if (ImGui::Button("Save engine settings"))
        {
            if (!settings.Save("engine://engine_settings.yaml", engineContext.GetFileSystem()))
            {
                LOGWARNING("EditorGui: failed to save engine settings.");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload engine settings"))
        {
            if (!settings.Load("engine://engine_settings.yaml", engineContext.GetFileSystem()))
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

    void EditorGui::RecordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem)
    {
        const auto virtualPath = fileSystem.ToVirtualPath(nativePath);
        if (!virtualPath)
        {
            LOGERROR("EditorGui::RecordLastScene: path is not under any registered mount (path: ", nativePath, ")");
            return;
        }

        m_Settings.LastScene = *virtualPath;
        m_Settings.Save("engine://editor_settings.yaml", fileSystem);
    }

    void EditorGui::LoadLastScene(HedgehogEngine::Engine& context)
    {
        if (m_Settings.LastScene.empty())
            return;

        if (!m_FileSystem->Exists(m_Settings.LastScene))
        {
            LOGWARNING("EditorGui::LoadLastScene: stored scene not found, starting with an empty scene (path: ",
                       m_Settings.LastScene, ")");
            m_Settings.LastScene.clear();
            m_Settings.Save("engine://editor_settings.yaml", *m_FileSystem);
            return;
        }

        // LoadScene expects a native path (the dialog's contract), so resolve back from virtual.
        const auto physicalPath = m_FileSystem->ResolvePhysical(m_Settings.LastScene);
        if (physicalPath && context.GetEngineContext().GetSceneManager().LoadScene(physicalPath->string()))
            return;

        LOGWARNING("EditorGui::LoadLastScene: failed to load stored scene, starting with an empty scene (path: ",
                   m_Settings.LastScene, ")");
        context.GetEngineContext().GetSceneManager().ResetScene();
        m_Settings.LastScene.clear();
        m_Settings.Save("engine://editor_settings.yaml", *m_FileSystem);
    }
}
