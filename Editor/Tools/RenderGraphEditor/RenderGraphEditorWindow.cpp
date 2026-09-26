#include "RenderGraphEditorWindow.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

namespace ed = ax::NodeEditor;

namespace Editor
{
    RenderGraphEditorWindow::~RenderGraphEditorWindow()
    {
        if (m_Canvas)
            ed::DestroyEditor(m_Canvas);
    }

    void RenderGraphEditorWindow::Draw()
    {
        if (!Open)
            return;

        if (!m_Canvas)
        {
            // No settings file: the canvas's view and node positions are not persisted by the
            // library. Graph layouts will be saved next to their .graph assets instead.
            ed::Config config;
            config.SettingsFile = nullptr;
            m_Canvas = ed::CreateEditor(&config);
        }

        ImGui::SetNextWindowSize({ 960.0f, 600.0f }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Render Graph Editor", &Open))
        {
            ImGui::TextDisabled("Right-drag to pan, mouse wheel to zoom.");
            ed::SetCurrentEditor(m_Canvas);
            ed::Begin("RenderGraphCanvas");
            ed::End();
            ed::SetCurrentEditor(nullptr);
        }
        ImGui::End();
    }
}
