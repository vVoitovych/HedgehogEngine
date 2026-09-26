#pragma once

namespace ax::NodeEditor
{
    struct EditorContext;
}

namespace Editor
{
    // The render graph editor (Tools > Render Graph Editor): a node canvas for .graph assets, drawn
    // with imgui-node-editor. For now it is an empty canvas that pans (right drag) and zooms
    // (mouse wheel); graphs appear on it in the next steps of the graph-editor epic.
    class RenderGraphEditorWindow
    {
    public:
        RenderGraphEditorWindow() = default;
        ~RenderGraphEditorWindow();

        RenderGraphEditorWindow(const RenderGraphEditorWindow&)            = delete;
        RenderGraphEditorWindow& operator=(const RenderGraphEditorWindow&) = delete;
        RenderGraphEditorWindow(RenderGraphEditorWindow&&)                 = delete;
        RenderGraphEditorWindow& operator=(RenderGraphEditorWindow&&)      = delete;

        bool Open = false;

        void Draw();

    private:
        // Created on the first Draw, inside a live ImGui frame, and destroyed with the window. That
        // happens after the editor has shut ImGui down, which is safe: destroying a canvas with no
        // settings file makes no ImGui calls.
        ax::NodeEditor::EditorContext* m_Canvas = nullptr;
    };
}
