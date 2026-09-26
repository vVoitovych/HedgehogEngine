#pragma once

#include "GraphCanvasModel.hpp"
#include "GraphLayoutFile.hpp"

#include "HedgehogRenderer/Graph/GraphAsset.hpp"

#include <filesystem>
#include <string>

namespace ax::NodeEditor
{
    struct EditorContext;
}

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    // The render graph editor (Tools > Render Graph Editor): a .graph asset drawn as nodes with
    // imgui-node-editor. Passes are nodes with a pin per slot; resources and imports feed the
    // passes' slots from the left, and slots bound to the graph's outputs lead to output nodes on
    // the right. Read-only for now: nodes can be moved (right-drag pans, the wheel zooms), and
    // their positions are kept in "<name>.graph.layout" beside the graph.
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

        // renderer supplies the graphs and pass types; with none, the window shows an empty canvas.
        void Draw(const Renderer::Renderer* renderer);

    private:
        void DrawGraphPicker(const Renderer::Renderer& renderer);
        // Rebuilds the model when the graph changes: another one picked, or a hot reload.
        void SyncGraph(const Renderer::Renderer& renderer);
        void DrawCanvas();
        void DrawNode(const GraphCanvasNode& node) const;
        // Once no node is being dragged, saves the layout if a node moved since it was placed.
        void SaveMovedNodes();

        // Created on the first Draw, inside a live ImGui frame, and destroyed with the window. That
        // happens after the editor has shut ImGui down, which is safe: destroying a canvas with no
        // settings file makes no ImGui calls.
        ax::NodeEditor::EditorContext* m_Canvas = nullptr;

        std::string           m_GraphName;
        bool                  m_HasGraph = false;
        Renderer::GraphAsset  m_Asset; // what m_Model was built from
        GraphCanvasModel      m_Model;
        std::filesystem::path m_LayoutFile;
        GraphLayout           m_Layout; // every node's position, as last saved or placed
        bool                  m_PlaceNodes = false; // set the nodes' positions on the next canvas frame
        int                   m_FitViewInFrames = 0; // counts down to fitting the view to the nodes
    };
}
