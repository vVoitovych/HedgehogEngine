#pragma once

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

        void Draw(Context::Context& context);

    private:
        // ── Layout ───────────────────────────────────────────────────────────
        void DrawEditorLayout(Context::Context& context, float W, float H, float menuH);
        void DrawLeftSplitter(float availH, float W);
        void DrawRightSplitter(float availH, float W);
        void DrawCenterColumn(Context::Context& context, float centerW, float availH);
        void DrawConsoleSplitter(float centerW, float availH);

        // ── Panel content (draw into the active child window) ────────────────
        void DrawMainMenu(Context::Context& context);
        void DrawSceneHierarchy(Context::Context& context);
        void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index);
        void DrawToolbar();
        void DrawInspector(Context::Context& context);
        void DrawEntityTitle(Context::Context& context);
        void DrawTransformComponent(Context::Context& context);
        void DrawMeshComponent(Context::Context& context);
        void DrawRenderComponent(Context::Context& context);
        void DrawLightComponent(Context::Context& context);
        void DrawScriptComponent(Context::Context& context);

        // ── Floating dialogs (own Begin/End) ─────────────────────────────────
        void DrawSettingsWindow(Context::Context& context);

    private:
        // Layout constants
        static constexpr float k_SplitterThickness = 5.0f;
        static constexpr float k_ToolbarHeight     = 36.0f;
        static constexpr float k_MinPanelSize      = 100.0f;

        // Runtime-resizable panel sizes
        float m_LeftPanelWidth     = 300.0f;
        float m_RightPanelWidth    = 300.0f;
        float m_ConsolePanelHeight = 200.0f;

        std::optional<ECS::Entity>    m_SelectedEntity;
        EditorMode                    m_EditorMode         = EditorMode::Edit;
        bool                          m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel> m_ConsolePanel;
    };
}
