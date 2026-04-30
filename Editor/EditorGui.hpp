#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "ECS/api/Entity.hpp"

#include <memory>
#include <optional>

namespace HedgehogEngine
{
    class HedgehogEngine;
}

namespace Editor
{
    class ConsolePanel;

    enum class EditorMode { Edit, Play, Pause };

    class EditorGui
    {
    public:
        EditorGui();
        ~EditorGui();

        EditorGui(const EditorGui&)            = delete;
        EditorGui& operator=(const EditorGui&) = delete;
        EditorGui(EditorGui&&)                 = delete;
        EditorGui& operator=(EditorGui&&)      = delete;

        void Draw(HedgehogEngine::HedgehogEngine& context, void* sceneViewTextureId);

        uint32_t GetSceneViewWidth()    const { return m_SceneViewWidth; }
        uint32_t GetSceneViewHeight()   const { return m_SceneViewHeight; }
        bool     IsSceneViewHovered()   const { return m_SceneViewHovered; }

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, HedgehogEngine::HedgehogEngine& context, void* sceneViewTextureId);
        void DrawMainMenu(HedgehogEngine::HedgehogEngine& context);
        void DrawToolbarContent();
        void DrawSceneViewContent(void* sceneViewTextureId);
        void DrawSceneHierarchy(HedgehogEngine::HedgehogEngine& context);
        void DrawHierarchyNode(HedgehogEngine::HedgehogEngine& context, ECS::Entity entity, int& index);
        void DrawInspector(HedgehogEngine::HedgehogEngine& context);
        void DrawEntityTitle(HedgehogEngine::HedgehogEngine& context);
        void DrawTransformComponent(HedgehogEngine::HedgehogEngine& context);
        void DrawMeshComponent(HedgehogEngine::HedgehogEngine& context);
        void DrawRenderComponent(HedgehogEngine::HedgehogEngine& context);
        void DrawLightComponent(HedgehogEngine::HedgehogEngine& context);
        void DrawScriptComponent(HedgehogEngine::HedgehogEngine& context);

        // ── Floating dialogs ─────────────────────────────────────────────────
        void DrawSettingsWindow(HedgehogEngine::HedgehogEngine& context);

    private:
        static constexpr const char k_SettingsPath[] = "editor_settings.yaml";

        EditorSettings m_Settings;
        DockSystem     m_DockSystem;

        uint32_t m_SceneViewWidth   = 0;
        uint32_t m_SceneViewHeight  = 0;
        bool     m_SceneViewHovered = false;

        std::optional<ECS::Entity>    m_SelectedEntity;
        EditorMode                    m_EditorMode         = EditorMode::Edit;
        bool                          m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel> m_ConsolePanel;

        // Non-owning pointer valid only during Draw(); used by DrawPanelContent lambdas
        void* m_SceneViewTextureId = nullptr;
    };
}
