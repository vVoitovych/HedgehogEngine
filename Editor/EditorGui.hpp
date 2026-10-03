#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "Panels/ContentPanel.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogInput/api/GameInputRegion.hpp"
#include "HedgehogScripting/api/ScriptPropertyDeclaration.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace HedgehogEngine
{
    class Engine;
}

namespace Renderer
{
    class Renderer;
}

namespace HedgehogScripting
{
    class ScriptSystem;
}

namespace Editor
{
    class ConsolePanel;
    class VertexDescriptionWindow;
    class PipelineWindow;
    class ShaderWindow;
    class RenderGraphEditorWindow;

    // The graphs the editor's own views use (RENDERING.md section 7). The inspector lists them
    // apart from the graphs a scene camera would normally pick.
    inline constexpr const char* SCENE_GRAPH  = "scene";
    inline constexpr const char* RESULT_GRAPH = "result";

    // What the scene and game panels show this frame: ImGui texture ids (nullptr: nothing to show)
    // and, on the render-graph path, how many passes the last frame ran; with the Content panel's
    // icon for each type.
    struct ViewportImages
    {
        void*          Scene          = nullptr;
        void*          Game           = nullptr;
        size_t         GraphPassCount = 0;
        ContentIconIds ContentIcons   = {};
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
        explicit EditorGui(HedgehogEngine::Engine& context);
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

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context);
        void DrawMainMenu(HedgehogEngine::Engine& context);
        void DrawToolbarContent(HedgehogEngine::Engine& context);
        void DrawSceneViewContent();
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
        // A mesh dropped on the hierarchy: a new entity named after the file, with Mesh and Render
        // components (the scene's first material), under parent or the root.
        void CreateMeshEntity(HedgehogEngine::Engine& context, const ContentOpenRequest& mesh,
                              std::optional<ECS::Entity> parent);
        void DrawOpenSceneDropPopup(HedgehogEngine::Engine& context);
        // False when the selected entity has nowhere to put the file.
        bool AssignToSelection(HedgehogEngine::Engine& context, const ContentOpenRequest& request,
                               const std::string& physicalPath);
        // Edit mode only: points entity's ScriptComponent at the script and lists its parameters,
        // running none of its code. False, logged, when refused.
        bool AssignScript(HedgehogEngine::Engine& context, ECS::Entity entity, const std::string& physicalPath);
        void DrawSceneHierarchy(HedgehogEngine::Engine& context);
        void DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, int& index);
        void DrawInspector(HedgehogEngine::Engine& context);
        void DrawEntityTitle(HedgehogEngine::Engine& context);
        void DrawTransformComponent(HedgehogEngine::Engine& context);
        void DrawMeshComponent(HedgehogEngine::Engine& context);
        void DrawRenderComponent(HedgehogEngine::Engine& context);
        void DrawLightComponent(HedgehogEngine::Engine& context);
        void DrawCameraComponent(HedgehogEngine::Engine& context);
        void DrawCameraGraph(std::string& graphName);
        void DrawScriptComponent(HedgehogEngine::Engine& context);
        void DrawAnimatorComponent(HedgehogEngine::Engine& context);
        // The five game UI components, each with its reflected fields and a Remove button.
        void DrawUiComponents(HedgehogEngine::Engine& context);
        // The script's property declarations for the inspector, described again only when the
        // file changes on disk; nullptr when there is no script system or no such file.
        const std::vector<HedgehogScripting::ScriptPropertyDeclaration>* FindScriptDeclarations(
            const FS::FileSystemManager& fileSystem, const std::string& scriptPath);

        // ── Floating dialogs ─────────────────────────────────────────────────
        void DrawSettingsWindow(HedgehogEngine::Engine& context);

        // ── Last-scene persistence ───────────────────────────────────────────
        void RecordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem);
        void LoadLastScene(HedgehogEngine::Engine& context);

    private:
        // Non-owning pointer to the FileSystemManager; valid for the entire lifetime of EditorGui
        // because the engine context outlives it (destroyed first among Application's members).
        const FS::FileSystemManager* m_FileSystem = nullptr;

        EditorSettings m_Settings;
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
        bool                                   m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel>            m_ConsolePanel;
        std::unique_ptr<ContentPanel>            m_ContentPanel;
        std::unique_ptr<VertexDescriptionWindow> m_VertexDescWindow;
        std::unique_ptr<PipelineWindow>          m_PipelineWindow;
        std::unique_ptr<ShaderWindow>            m_ShaderWindow;
        std::unique_ptr<RenderGraphEditorWindow> m_RenderGraphEditorWindow;

        // Valid only during Draw(); read by the viewport panel.
        ViewportImages m_ViewportImages;
        Renderer::Renderer*          m_Renderer = nullptr;
        HedgehogScripting::ScriptSystem* m_ScriptSystem = nullptr;
        std::optional<AssetDrop>          m_AssetDrop;        // this frame's, applied after the panels
        std::optional<ContentOpenRequest> m_SceneToOpen;      // a scene dropped on the hierarchy, awaiting yes

        // What DescribeScript last returned for each script, by assets:// path, with the file's
        // write time then: the inspector draws every frame, so a script is described once per change.
        struct ScriptDeclarations
        {
            std::filesystem::file_time_type                          WriteTime;
            std::vector<HedgehogScripting::ScriptPropertyDeclaration> Declarations;
        };
        std::unordered_map<std::string, ScriptDeclarations> m_ScriptDeclarations;
        bool                         m_Benchmarking = false;
    };
}
