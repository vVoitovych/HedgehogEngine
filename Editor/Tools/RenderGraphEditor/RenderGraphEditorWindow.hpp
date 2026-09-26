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
    // imgui-node-editor, and edited there. Passes are nodes with a pin per slot; resources and
    // imports feed the passes' slots from the left, and slots bound to the graph's outputs lead to
    // output nodes on the right.
    //
    // Editing: right-click the canvas to add a pass, resource, import or output; drag between a
    // resource, import or output pin and a pass's slot pin to bind the slot; select a link or node
    // and press Delete to remove it. Save (Ctrl+S) writes the .graph with GraphAssetWriter, which
    // the renderer then hot-reloads; a graph that fails its reload keeps rendering its last good
    // version, and the error is shown here. New... starts a graph from a copy of "game". Node
    // positions are kept in "<name>.graph.layout".
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
        // What waits for the unsaved-changes prompt to be answered.
        enum class PendingAction
        {
            None,
            OpenGraph, // switch to m_PendingGraph
            Close,
        };

        // ── Graph lifetime ───────────────────────────────────────────────────
        void DrawToolbar(const Renderer::Renderer& renderer);
        void DrawNewGraphPopup(const Renderer::Renderer& renderer);
        void DrawUnsavedChangesPopup(const Renderer::Renderer& renderer);
        // Opens the graph at once, or asks first when there are unsaved edits.
        void RequestOpenGraph(const std::string& name);
        void OpenGraph(const std::string& name);
        // Loads the graph when it changes: another one opened, or the renderer reloaded it.
        void SyncGraph(const Renderer::Renderer& renderer);
        void Save(const Renderer::Renderer& renderer);
        bool IsDirty() const { return m_HasGraph && !(m_Edited == m_Saved); }

        // ── Canvas ───────────────────────────────────────────────────────────
        void DrawCanvas(const Renderer::Renderer* renderer);
        void DrawNode(const GraphCanvasNode& node) const;
        void DrawAddMenu(const Renderer::Renderer& renderer);
        void HandleNewLinks();
        void HandleDeletions();
        // Once no node is being dragged, saves the layout if a node moved since it was placed.
        void SaveMovedNodes();

        // Rebuilds the canvas from m_Edited after an edit, keeping every node where it is. Pass
        // nodesChanged when a node was added or removed, which renumbers the canvas's ids.
        void ApplyEdit(bool nodesChanged);
        const GraphCanvasNode* FindNode(uint32_t id) const;
        const GraphCanvasPin*  FindPin(uint32_t id, const GraphCanvasNode** owner) const;

        // Created on the first Draw, inside a live ImGui frame, and destroyed with the window. That
        // happens after the editor has shut ImGui down, which is safe: destroying a canvas with no
        // settings file makes no ImGui calls.
        ax::NodeEditor::EditorContext* m_Canvas = nullptr;
        const Renderer::Renderer*      m_Renderer = nullptr; // this frame's, for pass types

        std::string           m_GraphName;
        bool                  m_HasGraph = false;
        Renderer::GraphAsset  m_Edited;  // what the canvas shows and edits
        Renderer::GraphAsset  m_Saved;   // what the file holds, as last loaded or saved
        Renderer::GraphAsset  m_Library; // the renderer's asset as last seen, to notice its reloads
        std::filesystem::path m_GraphFile;
        GraphCanvasModel      m_Model;
        GraphLayout           m_Layout;  // every node's position, as last saved or placed
        std::string           m_Status;  // the last save's outcome or an edit's error

        bool m_PlaceNodes      = false; // set the nodes' positions on the next canvas frame
        bool m_ClearSelection  = false; // on the next canvas frame, after ids were renumbered
        bool m_FitAfterPlacing = false; // and fit the view to them: only when a graph is opened
        int  m_FitViewInFrames = 0;     // counts down to fitting the view

        PendingAction     m_Pending = PendingAction::None;
        std::string       m_PendingGraph;
        GraphNodePosition m_AddPosition; // canvas position of the right-click that opened the Add menu
        char              m_NewGraphName[64] = {};
    };
}
