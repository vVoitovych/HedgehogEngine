#pragma once

#include "ECS/api/Entity.hpp"

#include <optional>

namespace Context
{
    class Context;
}

namespace HedgehogClient
{
    class EditorGui
    {
    public:
        EditorGui()  = default;
        ~EditorGui() = default;

        EditorGui(const EditorGui&)            = delete;
        EditorGui& operator=(const EditorGui&) = delete;

        void Draw(Context::Context& context);

        void ShowSettingsWindow();

    private:
        void DrawGui(Context::Context& context);

        void DrawInspector(Context::Context& context);
        void DrawTitle(Context::Context& context);
        void DrawTransform(Context::Context& context);
        void DrawMesh(Context::Context& context);
        void DrawRender(Context::Context& context);
        void DrawLight(Context::Context& context);
        void DrawScript(Context::Context& context);

        void DrawMainMenu(Context::Context& context);
        void DrawSceneInspector(Context::Context& context);
        void DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index);

        void DrawSettingsWindow(Context::Context& context);

    private:
        std::optional<ECS::Entity> m_SelectedObject;
        bool                       m_SettingsWindowShow = false;
    };
}
