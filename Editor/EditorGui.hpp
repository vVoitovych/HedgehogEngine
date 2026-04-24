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
        void DrawMainMenu(Context::Context& context);
        void DrawSceneHierarchy(Context::Context& context);
        void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index);
        void DrawToolbar();
        void DrawConsolePanel();
        void DrawInspector(Context::Context& context);

        // Inspector component sections
        void DrawEntityTitle(Context::Context& context);
        void DrawTransformComponent(Context::Context& context);
        void DrawMeshComponent(Context::Context& context);
        void DrawRenderComponent(Context::Context& context);
        void DrawLightComponent(Context::Context& context);
        void DrawScriptComponent(Context::Context& context);

        void DrawSettingsWindow(Context::Context& context);

    private:
        static constexpr float k_LeftPanelWidth     = 300.0f;
        static constexpr float k_RightPanelWidth    = 300.0f;
        static constexpr float k_ToolbarHeight      = 35.0f;
        static constexpr float k_ConsolePanelHeight = 200.0f;

        std::optional<ECS::Entity>    m_SelectedEntity;
        EditorMode                    m_EditorMode         = EditorMode::Edit;
        bool                          m_SettingsWindowOpen = false;
        std::unique_ptr<ConsolePanel> m_ConsolePanel;
    };
}
