#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "Panels/ContentPanel.hpp"
#include "Panels/EditorIcons.hpp"
#include "Reflection/GuiReflection.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "EcsSerialization/api/Prefab/OverrideSet.hpp"

#include "HedgehogAudio/api/SoundHandle.hpp"
#include "HedgehogInput/api/GameInputRegion.hpp"
#include "HedgehogScripting/api/ScriptPropertyDeclaration.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ImFont;

namespace ECS
{
    class ECS;
}

namespace HedgehogEngine
{
    class Engine;
    class EngineContext;
}

namespace Renderer
{
    class Renderer;
}

namespace HedgehogScripting
{
    class ScriptSystem;
}

namespace EcsSerialization
{
    struct ComponentInfo;
}

namespace Editor
{
    class ConsolePanel;
    class VertexDescriptionWindow;
    class PipelineWindow;
    class ShaderWindow;
    class InputActionsWindow;
    class NewProjectWindow;
    class ProjectSettingsWindow;
    class RenderGraphEditorWindow;

    // The graphs the editor's own views use (RENDERING.md section 7). The inspector lists them
    // apart from the graphs a scene camera would normally pick.
    inline constexpr const char* SCENE_GRAPH  = "scene";
    inline constexpr const char* RESULT_GRAPH = "result";

    // What the scene and game panels show this frame: ImGui texture ids (nullptr: nothing to show)
    // and, on the render-graph path, how many passes the last frame ran; with the Content panel's
    // icon for each type and the editor's line icons.
    struct ViewportImages
    {
        void*          Scene          = nullptr;
        void*          Game           = nullptr;
        size_t         GraphPassCount = 0;
        ContentIconIds ContentIcons   = {};
        EditorIconIds  EditorIcons    = {};
    };

    // A point in a panel's image: (0, 0) its top left corner, (1, 1) its bottom right.
    struct ViewportPoint
    {
        float U = 0.0f;
        float V = 0.0f;
    };

    class EditorGui
    {
    public:
        // recordRecentProject: put the open project at the front of the recent list (the
        // interactive editor); otherwise the list keeps its order and the project its entry, if any.
        EditorGui(HedgehogEngine::Engine& context, bool recordRecentProject);
        ~EditorGui();

        EditorGui(const EditorGui&)            = delete;
        EditorGui& operator=(const EditorGui&) = delete;
        EditorGui(EditorGui&&)                 = delete;
        EditorGui& operator=(EditorGui&&)      = delete;

        void Draw(HedgehogEngine::Engine& context, const ViewportImages& images);

        // A panel's size is 0x0 while its tab is hidden, so its view is dropped for the frame.
        uint32_t GetSceneViewWidth()    const { return m_SceneViewWidth; }
        uint32_t GetSceneViewHeight()   const { return m_SceneViewHeight; }
        uint32_t GetGameViewWidth()     const { return m_GameViewWidth; }
        uint32_t GetGameViewHeight()    const { return m_GameViewHeight; }
        bool     IsSceneViewHovered()   const { return m_SceneViewHovered; }

        // The Game tab's image as the game's input region (in window coordinates, the cursor's), as
        // of the last Draw: the pointer is the game's while the image is hovered, the keyboard while
        // the panel has focus (gained by a press on the image, lost by a press anywhere else or by
        // hiding the tab) and no text field is being edited. No area while the tab is hidden.
        HInput::GameInputRegion GetGameInputRegion() const;

        // The Scene tab's image as the editor camera's input region, by the same rules: the pointer
        // while the image is hovered (a drag that began on it keeps its button), the keyboard while
        // the scene panel has focus and no text field is being edited.
        HInput::GameInputRegion GetSceneInputRegion() const;

        // Where the scene panel was clicked this frame (pressed and released without dragging the
        // camera), for the application to pick at.
        std::optional<ViewportPoint> GetScenePick() const { return m_ScenePick; }

        std::optional<ECS::Entity> GetSelectedEntity() const { return m_SelectedEntity; }
        void SetSelectedEntity(std::optional<ECS::Entity> entity) { m_SelectedEntity = entity; }

        // The renderer the render graph editor and the camera inspector read graphs and pass types
        // from, and register the graph files they open with; set once it exists.
        void SetRenderer(Renderer::Renderer* renderer) { m_Renderer = renderer; }

        // The script system the inspector lists script parameters with and pushes live edits to.
        // Not owned: the engine's ECS owns it.
        void SetScriptSystem(HedgehogScripting::ScriptSystem* scriptSystem) { m_ScriptSystem = scriptSystem; }

        // A benchmark measures the renderer: the Content panel then draws nothing, so its folder
        // scans never land in the numbers, whatever tab the saved layout left active.
        void SetBenchmarkMode(bool benchmarking) { m_Benchmarking = benchmarking; }

        // The Console's monospace font (ImGuiLayer::GetMonoFont); nullptr keeps the UI font.
        void SetMonoFont(ImFont* font) { m_MonoFont = font; }

        // The project File > Open Project... or Recent Projects asked for, once confirmed (taken
        // once; nullopt otherwise). The application then rebuilds the editor on it.
        std::optional<std::filesystem::path> TakeProjectRequest();

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context);
        void DrawMainMenu(HedgehogEngine::Engine& context);
        // File > New Project..., Open Project... and Recent Projects, enabled in Edit mode only.
        void DrawProjectMenuItems(bool editing);
        // Checks folder holds a readable Project.yaml (else one error in the Console), asks before
        // leaving the open project unless confirm is false (a project New Project just created,
        // whose window says so), then records it first in the recent list and requests it.
        void RequestProject(const std::filesystem::path& folder, bool confirm = true);
        // The Add component items, enabled while an entity is selected; shared with the inspector.
        void DrawAddComponentItems(HedgehogEngine::Engine& context);
        void CreateMaterial(HedgehogEngine::EngineContext& engineContext);
        void DrawToolbarContent(HedgehogEngine::Engine& context);
        // The icon's ImGui texture id, or nullptr when it did not load.
        [[nodiscard]] void* GetIcon(EditorIcon icon) const { return m_ViewportImages.EditorIcons[static_cast<size_t>(icon)]; }
        void DrawSceneViewContent(HedgehogEngine::Engine& context);
        void DrawContentPanel(HedgehogEngine::Engine& context);
        // What opening a Content panel file means, by type (EditorGuiContent.cpp): a scene
        // loads; a mesh, material, texture or script goes onto the selected entity; a shader,
        // pipeline, vertex description or graph opens in its tool; anything else, or an asset
        // with nothing suitable selected, opens in the OS default application.
        void OpenContentItem(HedgehogEngine::Engine& context, const ContentOpenRequest& request);
        // Drops arrive while panels draw, over lists they are iterating: each is kept here and
        // applied once the frame's panels are drawn (EditorGuiContent.cpp).
        struct AssetDrop
        {
            ContentOpenRequest         Asset;
            bool                       OnHierarchy = false; // else onto the selected entity
            std::optional<ECS::Entity> Parent;              // the hierarchy entity it was dropped on
        };
        void ApplyAssetDrop(HedgehogEngine::Engine& context);
        // Makes the last inspector widget a drop target for assets of that type, onto the selection.
        void AcceptSelectionDrop(ContentType type);
        // Plays an audio clip from the Content panel as a 2D one-shot, or stops it when it is the
        // clip already playing.
        void PreviewAudio(HedgehogEngine::EngineContext& engineContext, const std::string& virtualPath);
        // A mesh dropped on the hierarchy: a new entity named after the file, with Mesh and Render
        // components (the scene's first material), under parent or the root.
        void CreateMeshEntity(HedgehogEngine::Engine& context, const ContentOpenRequest& mesh,
                              std::optional<ECS::Entity> parent);
        // A prefab dropped on the hierarchy (under parent) or opened (at the root): a new instance,
        // selected.
        void InstantiatePrefab(HedgehogEngine::Engine& context, const std::string& virtualPath,
                               std::optional<ECS::Entity> parent);
        // Hierarchy > Create Prefab...: asks where under assets:// to save entity's subtree
        // (Prefabs/<name>.prefab suggested), writes it and makes entity an instance of it.
        void CreatePrefabFrom(HedgehogEngine::Engine& context, ECS::Entity entity);
        void DrawOpenSceneDropPopup(HedgehogEngine::Engine& context);
        // False when the selected entity has nowhere to put the file.
        bool AssignToSelection(HedgehogEngine::Engine& context, const ContentOpenRequest& request,
                               const std::string& physicalPath);
        // Edit mode only: points entity's ScriptComponent at the script and lists its parameters,
        // running none of its code. False, logged, when refused.
        bool AssignScript(HedgehogEngine::Engine& context, ECS::Entity entity, const std::string& physicalPath);
        void DrawSceneHierarchy(HedgehogEngine::Engine& context);
        void DrawDropRow(const char* label, const char* hint, ContentType type);
        // Draws entity's row and, when open, its children; filtering shows only marked entities.
        void DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, bool filtering);
        // Marks entity when its name or a descendant's contains search; returns whether it did.
        bool MarkHierarchyMatches(ECS::ECS& ecs, ECS::Entity entity, std::string_view search);
        void DrawInspector(HedgehogEngine::Engine& context);
        void DrawEntityTitle(HedgehogEngine::Engine& context);
        // For an entity of a prefab instance: the prefab's name, Select (in the Project panel),
        // Revert All and Apply All for the whole instance, and the entity's own overrides, each with
        // Revert and Apply (EditorGuiPrefabs.cpp).
        void DrawPrefabBar(HedgehogEngine::Engine& context);
        // Recomputes the selected instance's overrides when they may have changed.
        void RefreshPrefabOverrides(HedgehogEngine::Engine& context);
        // Reverts or applies the given overrides of the selected entity's instance.
        void ChangePrefabOverrides(HedgehogEngine::Engine& context, const EcsSerialization::OverrideSet& overrides, bool apply);
        // The overridden row a reflected row's menu asked to revert or apply.
        void HandlePrefabRowRequest(HedgehogEngine::Engine& context);
        void DrawTransformComponent(HedgehogEngine::Engine& context);
        void DrawMeshComponent(HedgehogEngine::Engine& context);
        void DrawRenderComponent(HedgehogEngine::Engine& context);
        void DrawLightComponent(HedgehogEngine::Engine& context);
        void DrawCameraComponent(HedgehogEngine::Engine& context);
        void DrawCameraGraph(std::string& graphName);
        void DrawScriptComponent(HedgehogEngine::Engine& context);
        void DrawAnimatorComponent(HedgehogEngine::Engine& context);
        // A registered component with no hand-drawn section: its icon header (enable checkbox,
        // Remove) and reflected rows, then its extra rows (m_InspectorExtraRows).
        void DrawRegisteredComponent(HedgehogEngine::Engine& context, const EcsSerialization::ComponentInfo& info);
        // The script's property declarations for the inspector, described again only when the
        // file changes on disk; nullptr when there is no script system or no such file.
        const std::vector<HedgehogScripting::ScriptPropertyDeclaration>* FindScriptDeclarations(
            const FS::FileSystemManager& fileSystem, const std::string& scriptPath);

        // ── Floating dialogs ─────────────────────────────────────────────────
        void DrawSettingsWindow(HedgehogEngine::Engine& context);

        // ── Last-scene persistence ───────────────────────────────────────────
        // The open project's entry in the recent list (added when missing and recorded, else a
        // scratch entry outside the list): its LastScene is the scene this project reopens with.
        RecentProject& CurrentProject();
        void RecordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem);
        void LoadLastScene(HedgehogEngine::Engine& context);

    private:
        // Non-owning pointer to the FileSystemManager; valid for the entire lifetime of EditorGui
        // because the engine context outlives it (destroyed first among Application's members).
        const FS::FileSystemManager* m_FileSystem = nullptr;

        EditorSettings m_Settings;
        std::filesystem::path m_ProjectRoot; // the open project's folder
        bool                  m_RecordRecentProject = false;
        RecentProject         m_UnrecordedProject; // CurrentProject() of a project not in the list
        std::optional<std::filesystem::path> m_ProjectRequest;
        DockSystem     m_DockSystem;

        uint32_t m_SceneViewWidth   = 0;
        uint32_t m_SceneViewHeight  = 0;
        uint32_t m_GameViewWidth    = 0;
        uint32_t m_GameViewHeight   = 0;
        bool     m_SceneViewHovered = false;
        HM::Vector2 m_SceneImageMin  = HM::Vector2(0.0f, 0.0f); // window coordinates
        HM::Vector2 m_SceneImageSize = HM::Vector2(0.0f, 0.0f);
        bool        m_SceneViewFocused = false;
        HM::Vector2 m_GameImageMin  = HM::Vector2(0.0f, 0.0f); // window coordinates
        HM::Vector2 m_GameImageSize = HM::Vector2(0.0f, 0.0f);
        bool     m_GameViewHovered  = false;
        bool     m_GameViewFocused  = false;
        std::optional<ViewportPoint> m_ScenePick;

        std::optional<ECS::Entity>             m_SelectedEntity;

        // The hierarchy's search text, which entities it shows (one flag per entity id, sized once),
        // and the edits picked in a row's menu, applied after the tree is drawn.
        char                       m_HierarchySearch[128] = {};
        std::vector<uint8_t>       m_HierarchyMatches;
        std::optional<ECS::Entity> m_HierarchyCreateUnder;
        std::optional<ECS::Entity> m_HierarchyDelete;
        std::optional<ECS::Entity> m_HierarchyCreatePrefab;
        bool                                   m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel>            m_ConsolePanel;
        std::unique_ptr<ContentPanel>            m_ContentPanel;
        std::unique_ptr<VertexDescriptionWindow> m_VertexDescWindow;
        std::unique_ptr<PipelineWindow>          m_PipelineWindow;
        std::unique_ptr<ShaderWindow>            m_ShaderWindow;
        std::unique_ptr<InputActionsWindow>      m_InputActionsWindow;
        std::unique_ptr<ProjectSettingsWindow>   m_ProjectSettingsWindow;
        std::unique_ptr<NewProjectWindow>        m_NewProjectWindow;
        std::unique_ptr<RenderGraphEditorWindow> m_RenderGraphEditorWindow;

        // Valid only during Draw(); read by the viewport panel.
        ViewportImages m_ViewportImages;
        Renderer::Renderer*          m_Renderer = nullptr;
        HedgehogScripting::ScriptSystem* m_ScriptSystem = nullptr;
        std::optional<AssetDrop>          m_AssetDrop;        // this frame's, applied after the panels
        std::optional<ContentOpenRequest> m_SceneToOpen;      // a scene dropped on the hierarchy, awaiting yes

        // The selected entity's prefab instance and its overrides (every node of it), recomputed
        // when the selection changes, after an inspector edit, a revert or an apply, and every frame
        // outside Edit; the marks hand the selected node's overrides to the reflected rows.
        struct PrefabOverrideCache
        {
            std::optional<ECS::Entity>    Entity;
            ECS::Entity                   InstanceRoot = ECS::INVALID_ENTITY;
            uint32_t                      LocalId      = 0;
            EcsSerialization::OverrideSet Instance;
            bool                          Stale = true;
        };
        PrefabOverrideCache               m_PrefabOverrides;
        Reflection::PrefabOverrideMarks   m_PrefabMarks;

        // The inspector's hand-drawn sections and the generic sections' extra rows, by component key.
        using InspectorDrawer = void (EditorGui::*)(HedgehogEngine::Engine&);
        std::unordered_map<std::string, InspectorDrawer>       m_InspectorDrawers;
        std::unordered_map<std::string, std::function<void()>> m_InspectorExtraRows;
        // Types whose enabled property's row is already hidden behind the header's checkbox.
        std::unordered_set<std::string>                        m_HiddenEnabledRows;
        HA::SoundHandle                   m_PreviewSound;     // the Content panel's audio preview
        std::string                       m_PreviewPath;

        // What DescribeScript last returned for each script, by assets:// path, with the file's
        // write time then: the inspector draws every frame, so a script is described once per change.
        struct ScriptDeclarations
        {
            std::filesystem::file_time_type                          WriteTime;
            std::vector<HedgehogScripting::ScriptPropertyDeclaration> Declarations;
        };
        std::unordered_map<std::string, ScriptDeclarations> m_ScriptDeclarations;
        bool                         m_Benchmarking = false;
        ImFont*                      m_MonoFont     = nullptr;
    };
}
