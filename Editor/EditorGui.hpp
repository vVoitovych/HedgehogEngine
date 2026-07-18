#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "ECS/api/Entity.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <memory>
#include <optional>

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

        void Draw(HedgehogEngine::Engine& context, void* sceneViewTextureId);

        uint32_t GetSceneViewWidth()    const { return m_SceneViewWidth; }
        uint32_t GetSceneViewHeight()   const { return m_SceneViewHeight; }
        bool     IsSceneViewHovered()   const { return m_SceneViewHovered; }

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, HedgehogEngine::Engine& context, void* sceneViewTextureId);
        void DrawMainMenu(HedgehogEngine::Engine& context);
        void DrawToolbarContent();
        void DrawSceneViewContent(void* sceneViewTextureId);
        void DrawSceneHierarchy(HedgehogEngine::Engine& context);
        void DrawHierarchyNode(HedgehogEngine::Engine& context, ECS::Entity entity, int& index);
        void DrawInspector(HedgehogEngine::Engine& context);
        void DrawEntityTitle(HedgehogEngine::Engine& context);
        void DrawTransformComponent(HedgehogEngine::Engine& context);
        void DrawMeshComponent(HedgehogEngine::Engine& context);
        void DrawRenderComponent(HedgehogEngine::Engine& context);
        void DrawLightComponent(HedgehogEngine::Engine& context);
        void DrawScriptComponent(HedgehogEngine::Engine& context);

        // ── Floating dialogs ─────────────────────────────────────────────────
        void DrawSettingsWindow(HedgehogEngine::Engine& context);

        // ── Last-scene persistence ───────────────────────────────────────────
        void recordLastScene(const std::string& nativePath, const FS::FileSystemManager& fileSystem);
        void loadLastScene(HedgehogEngine::Engine& context);

    private:
        // Non-owning pointer to the FileSystemManager; valid for the entire lifetime of EditorGui
        // because the engine context outlives it (destroyed first among Application's members).
        const FS::FileSystemManager* m_FileSystem = nullptr;

        EditorSettings m_Settings;
        DockSystem     m_DockSystem;

        uint32_t m_SceneViewWidth   = 0;
        uint32_t m_SceneViewHeight  = 0;
        bool     m_SceneViewHovered = false;

        std::optional<ECS::Entity>             m_SelectedEntity;
        EditorMode                             m_EditorMode         = EditorMode::Edit;
        bool                                   m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel>            m_ConsolePanel;
        std::unique_ptr<VertexDescriptionWindow> m_VertexDescWindow;
        std::unique_ptr<PipelineWindow>          m_PipelineWindow;
        std::unique_ptr<ShaderWindow>            m_ShaderWindow;

        // Non-owning pointer valid only during Draw(); used by DrawPanelContent lambdas
        void* m_SceneViewTextureId = nullptr;
    };
}
