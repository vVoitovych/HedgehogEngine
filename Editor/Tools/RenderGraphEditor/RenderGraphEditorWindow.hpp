#pragma once

#include "GraphCanvasModel.hpp"
#include "GraphLayoutFile.hpp"

#include "HedgehogRenderer/Graph/GraphAsset.hpp"
#include "HedgehogRenderer/Graph/GraphDiagnostics.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace ax::NodeEditor
{
    struct EditorContext;
}

namespace FS
{
    class FileSystemManager;
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
    // and press Delete to remove it; edit the selected node in the Details panel.
    //
    // Files: New starts an empty, untitled graph; Open... opens any .graph file, registering it with
    // the renderer; the Graph drop-down switches between the graphs the renderer knows. Save
    // (Ctrl+S) writes the graph's file with GraphAssetWriter, or asks where for an untitled graph;
    // Save As... writes it to a new file and goes on editing that one. The renderer hot-reloads a
    // saved file; a graph that fails its reload keeps rendering its last good version, and the
    // error is shown here. Node positions are kept in "<name>.graph.layout" beside the graph.
    //
    // Every edit is checked the way the renderer would load it (Renderer::DiagnoseGraph): nodes
    // with a problem get a red border and a tooltip, and a list under the canvas names every
    // problem; clicking one selects its node and centres the view on it.
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
        // fileSystem names the files Open... and Save As... pick (MakeGraphReference).
        void Draw(Renderer::Renderer* renderer, const FS::FileSystemManager& fileSystem);

    private:
        // What waits for the unsaved-changes prompt to be answered.
        enum class PendingAction
        {
            None,
            OpenGraph, // switch to m_PendingGraph
            NewGraph,
            Close,
        };

        // ── Graph lifetime ───────────────────────────────────────────────────
        void DrawToolbar(Renderer::Renderer& renderer, const FS::FileSystemManager& fileSystem);
        void DrawUnsavedChangesPopup(Renderer::Renderer& renderer, const FS::FileSystemManager& fileSystem);
        // Acts at once, or asks first when there are unsaved edits.
        void RequestOpenGraph(const std::string& reference);
        void RequestNewGraph();
        void OpenGraph(const std::string& reference);
        void NewGraph();
        // Picks a .graph file, registers it with the renderer and opens it.
        void OpenFile(Renderer::Renderer& renderer, const FS::FileSystemManager& fileSystem);
        // Loads the graph when it changes: another one opened, or the renderer reloaded it.
        void SyncGraph(const Renderer::Renderer& renderer);
        // Save writes the graph's file; an untitled graph has none, so it saves as.
        void Save(Renderer::Renderer& renderer, const FS::FileSystemManager& fileSystem);
        void SaveAs(Renderer::Renderer& renderer, const FS::FileSystemManager& fileSystem);
        bool WriteGraph(const std::filesystem::path& file);
        bool IsUntitled() const { return m_HasGraph && m_GraphName.empty(); }
        bool IsDirty() const { return m_HasGraph && !(m_Edited == m_Saved); }

        // ── Canvas ───────────────────────────────────────────────────────────
        void DrawCanvas(const Renderer::Renderer* renderer);
        void DrawNode(const GraphCanvasNode& node) const;
        void DrawDiagnostics();
        // Checks m_Edited as the renderer would load it, and maps each problem to its node.
        void Diagnose();
        uint32_t FindDiagnosticNode(const Renderer::GraphDiagnostic& diagnostic) const;
        void DrawAddMenu(const Renderer::Renderer& renderer);
        void HandleNewLinks();
        void HandleDeletions();
        // Once no node is being dragged, saves the layout if a node moved since it was placed.
        void SaveMovedNodes();

        // ── Details panel (RenderGraphDetailsPanel.cpp) ──────────────────────
        void DrawDetails(const Renderer::Renderer& renderer);
        void DrawPassDetails(const Renderer::Renderer& renderer, const GraphCanvasNode& node);
        void DrawResourceDetails(const GraphCanvasNode& node);
        void DrawNameField(const GraphCanvasNode& node);

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

        std::string           m_GraphName; // the graph's reference; empty for an untitled graph
        bool                  m_HasGraph = false;
        Renderer::GraphAsset  m_Edited;  // what the canvas shows and edits
        Renderer::GraphAsset  m_Saved;   // what the file holds, as last loaded or saved
        Renderer::GraphAsset  m_Library; // the renderer's asset as last seen, to notice its reloads
        std::filesystem::path m_GraphFile; // empty for an untitled graph
        GraphCanvasModel      m_Model;
        GraphLayout           m_Layout;  // every node's position, as last saved or placed
        std::string           m_Status;  // the last save's outcome or an edit's error

        std::vector<Renderer::GraphDiagnostic>    m_Diagnostics;     // why m_Edited would not render
        std::vector<uint32_t>                     m_DiagnosticNodes; // per diagnostic, its node (0: none)
        std::unordered_map<uint32_t, std::string> m_NodeProblems;    // node id to its problems, one per line
        uint32_t                                  m_FocusNodeId = 0; // select and centre it next canvas frame

        bool m_PlaceNodes      = false; // set the nodes' positions on the next canvas frame
        bool m_ClearSelection  = false; // on the next canvas frame, after ids were renumbered
        bool m_FitAfterPlacing = false; // and fit the view to them: only when a graph is opened
        int  m_FitViewInFrames = 0;     // counts down to fitting the view

        PendingAction     m_Pending = PendingAction::None;
        std::string       m_PendingGraph;
        GraphNodePosition m_AddPosition; // canvas position of the right-click that opened the Add menu
    };
}
