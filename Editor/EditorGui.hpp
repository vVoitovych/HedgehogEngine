#pragma once

#include "Docking/DockSystem.hpp"
#include "EditorSettings.hpp"
#include "ECS/api/Entity.hpp"

#include <memory>
#include <optional>

namespace Context
{
    class Context;
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

        void Draw(Context::Context& context, void* sceneViewTextureId);

        uint32_t GetSceneViewWidth()    const { return m_SceneViewWidth; }
        uint32_t GetSceneViewHeight()   const { return m_SceneViewHeight; }
        bool     IsSceneViewHovered()   const { return m_SceneViewHovered; }

    private:
        // ── Panel content (drawn into dock areas) ────────────────────────────
        void DrawPanelContent(PanelId panel, Context::Context& context, void* sceneViewTextureId);
        void DrawMainMenu(Context::Context& context);
        void DrawToolbarContent();
        void DrawSceneViewContent(void* sceneViewTextureId);
        void DrawSceneHierarchy(Context::Context& context);
        void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index);
        void DrawInspector(Context::Context& context);
        void DrawEntityTitle(Context::Context& context);
        void DrawTransformComponent(Context::Context& context);
        void DrawMeshComponent(Context::Context& context);
        void DrawRenderComponent(Context::Context& context);
        void DrawLightComponent(Context::Context& context);
        void DrawScriptComponent(Context::Context& context);

        // ── Floating dialogs ─────────────────────────────────────────────────
        void DrawSettingsWindow(Context::Context& context);

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
