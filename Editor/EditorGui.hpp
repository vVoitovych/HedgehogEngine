#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <memory>
#include <optional>
#include <string>

namespace HedgehogEngine
{
    class Engine;
}

namespace Editor
{
    class ConsolePanel;
    class VertexDescriptionWindow;
    class PipelineWindow;
    class ShaderWindow;

    enum class EditorMode { Edit, Play, Pause };

    class EditorGui
    {
    public:
        explicit EditorGui(HedgehogEngine::Engine& context);
        ~EditorGui();

        EditorGui(const EditorGui&)            = delete;
        EditorGui& operator=(const EditorGui&) = delete;
        EditorGui(EditorGui&&)                 = delete;
        EditorGui& operator=(EditorGui&&)      = delete;

        void Draw(HedgehogEngine::Engine& context, void* sceneViewTextureId, void* gameViewTextureId);

        uint32_t GetSceneViewWidth()    const { return m_SceneViewWidth; }
        uint32_t GetSceneViewHeight()   const { return m_SceneViewHeight; }
        bool     IsSceneViewHovered()   const { return m_SceneViewHovered; }

        uint32_t GetGameViewWidth()     const { return m_GameViewWidth; }
        uint32_t GetGameViewHeight()    const { return m_GameViewHeight; }
        bool     IsGameViewVisible()    const { return m_GameViewVisible; }

        EditorMode GetEditorMode()      const { return m_EditorMode; }

        // World matrix of the currently selected entity, for the scene-view gizmo; nullopt if none.
        std::optional<HM::Matrix4x4> GetSelectedGizmoMatrix(HedgehogEngine::Engine& context) const;

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context);
        void DrawMainMenu(HedgehogEngine::Engine& context);
        void DrawToolbarContent(HedgehogEngine::Engine& context);
        void DrawSceneViewContent(HedgehogEngine::Engine& context);
        void DrawSceneHierarchy(HedgehogEngine::Engine& context);
        void DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, int& index);
        void DrawInspector(HedgehogEngine::Engine& context);
        void DrawEntityTitle(HedgehogEngine::Engine& context);
        void DrawTransformComponent(HedgehogEngine::Engine& context);
        void DrawMeshComponent(HedgehogEngine::Engine& context);
        void DrawRenderComponent(HedgehogEngine::Engine& context);
        void DrawLightComponent(HedgehogEngine::Engine& context);
        void DrawCameraComponent(HedgehogEngine::Engine& context);
        void DrawScriptComponent(HedgehogEngine::Engine& context);

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
        bool     m_SceneViewHovered = false;

        uint32_t m_GameViewWidth    = 0;
        uint32_t m_GameViewHeight   = 0;
        bool     m_GameViewVisible  = false;

        std::optional<ECS::Entity>             m_SelectedEntity;
        EditorMode                             m_EditorMode         = EditorMode::Edit;
        // Scene state captured on entering Play; restored on Stop so Play is non-destructive.
        std::string                            m_PlaySnapshot;
        bool                                   m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel>            m_ConsolePanel;
        std::unique_ptr<VertexDescriptionWindow> m_VertexDescWindow;
        std::unique_ptr<PipelineWindow>          m_PipelineWindow;
        std::unique_ptr<ShaderWindow>            m_ShaderWindow;

        // Non-owning pointers valid only during Draw(); used by DrawPanelContent lambdas
        void* m_SceneViewTextureId = nullptr;
        void* m_GameViewTextureId  = nullptr;
    };
}
