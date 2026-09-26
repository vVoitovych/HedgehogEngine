#include "RenderGraphEditorWindow.hpp"

#include "GraphEditing.hpp"
#include "RenderGraphEditorStyle.hpp"

#include "HedgehogRenderer/Graph/GraphAssetWriter.hpp"
#include "HedgehogRenderer/Renderer.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ed = ax::NodeEditor;

namespace Editor
{
    namespace
    {
        constexpr float COLUMN_WIDTH  = 300.0f;
        constexpr float ROW_HEIGHT    = 150.0f;
        constexpr float DETAILS_WIDTH = 320.0f;

        // A window that has just opened takes a few frames to settle its size, and fitting the view
        // before that is silently undone; nodes also have no size on the frame they are placed.
        constexpr int FIT_VIEW_DELAY_FRAMES = 10;

        constexpr const char* ADD_MENU_POPUP  = "AddGraphNode";
        constexpr const char* NEW_GRAPH_POPUP = "New render graph";
        constexpr const char* UNSAVED_POPUP   = "Unsaved graph changes";
        constexpr const char* TEMPLATE_GRAPH  = "game";

        ImVec4 TitleColor(const GraphCanvasNode& node)
        {
            if (node.Problem)
                return PROBLEM_COLOR;
            switch (node.Kind)
            {
            case GraphNodeKind::Resource: return RESOURCE_COLOR;
            case GraphNodeKind::Import:   return IMPORT_COLOR;
            case GraphNodeKind::Output:   return OUTPUT_COLOR;
            case GraphNodeKind::Pass:     return PASS_COLOR;
            }
            return PASS_COLOR;
        }

        GraphNodePosition AutomaticPosition(const GraphCanvasNode& node)
        {
            return { static_cast<float>(node.Column) * COLUMN_WIDTH, static_cast<float>(node.Row) * ROW_HEIGHT };
        }

        // A graph name is a file stem: letters, digits, '_' and '-'.
        bool IsValidGraphName(std::string_view name)
        {
            return !name.empty() && std::ranges::all_of(name, [](char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            });
        }

        // The renderer's load errors read "graph 'x' (file) is invalid; keeping ...:" and then one
        // indented line per problem: the first problem says the most in one line.
        std::string ErrorSummary(std::string_view error)
        {
            const size_t newline = error.find('\n');
            if (newline == std::string_view::npos)
                return std::string(error);
            std::string_view detail = error.substr(newline + 1);
            detail = detail.substr(0, detail.find('\n'));
            detail.remove_prefix(std::min(detail.find_first_not_of(' '), detail.size()));
            return std::string(detail);
        }
    }

    RenderGraphEditorWindow::~RenderGraphEditorWindow()
    {
        if (m_Canvas)
            ed::DestroyEditor(m_Canvas);
    }

    void RenderGraphEditorWindow::Draw(const Renderer::Renderer* renderer)
    {
        if (!Open)
            return;
        m_Renderer = renderer;

        if (!m_Canvas)
        {
            // No settings file: node positions live in the graph's own layout file instead.
            ed::Config config;
            config.SettingsFile = nullptr;
            m_Canvas = ed::CreateEditor(&config);
        }

        std::string title = "Render Graph Editor";
        if (!m_GraphName.empty())
            title += " - " + m_GraphName + (IsDirty() ? "*" : "");
        title += "###RenderGraphEditor";

        bool keepOpen = true;
        ImGui::SetNextWindowSize({ 1200.0f, 640.0f }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &keepOpen))
        {
            if (renderer)
            {
                DrawToolbar(*renderer);
                SyncGraph(*renderer);
            }

            ImGui::BeginChild("RenderGraphCanvasRegion", ImVec2(-DETAILS_WIDTH, 0.0f));
            DrawCanvas(renderer);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("RenderGraphDetails", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
            if (renderer)
                DrawDetails(*renderer);
            ImGui::EndChild();

            if (renderer)
            {
                if (IsDirty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                    && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
                {
                    Save(*renderer);
                }
                DrawNewGraphPopup(*renderer);
                DrawUnsavedChangesPopup(*renderer);
            }
        }
        ImGui::End();

        if (!keepOpen)
        {
            if (IsDirty())
                m_Pending = PendingAction::Close; // the prompt opens with the window next frame
            else
                Open = false;
        }
    }

    // ── Graph lifetime ───────────────────────────────────────────────────────

    void RenderGraphEditorWindow::DrawToolbar(const Renderer::Renderer& renderer)
    {
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Graph", m_GraphName.empty() ? "(open a graph)" : m_GraphName.c_str()))
        {
            for (const std::string& name : renderer.GetGraphNames())
            {
                const bool chosen = name == m_GraphName;
                if (ImGui::Selectable(name.c_str(), chosen))
                    RequestOpenGraph(name);
                if (chosen)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("New..."))
        {
            m_NewGraphName[0] = '\0';
            ImGui::OpenPopup(NEW_GRAPH_POPUP);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsDirty());
        if (ImGui::Button("Save"))
            Save(renderer);
        ImGui::EndDisabled();

        // What went wrong wins over what went right: a graph the renderer rejected, then the last
        // save's or edit's message, then a hint.
        const std::string_view error = m_GraphName.empty() ? std::string_view{} : renderer.GetGraphError(m_GraphName);
        if (!error.empty())
        {
            ImGui::TextColored(PROBLEM_COLOR, "Rejected by the renderer, which keeps the last good version: %s",
                               ErrorSummary(error).c_str());
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", std::string(error).c_str());
        }
        else if (!m_Status.empty())
            ImGui::TextUnformatted(m_Status.c_str());
        else if (!m_HasGraph && !m_GraphName.empty())
            ImGui::TextDisabled("Loading '%s'...", m_GraphName.c_str());
        else
            ImGui::TextDisabled("Right-click to add nodes, drag between pins to bind, Delete removes the selection.");
    }

    void RenderGraphEditorWindow::DrawNewGraphPopup(const Renderer::Renderer& renderer)
    {
        if (!ImGui::BeginPopupModal(NEW_GRAPH_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::TextUnformatted("The new graph starts as a copy of 'game'.");
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputText("Name", m_NewGraphName, sizeof(m_NewGraphName));

        const std::string name(m_NewGraphName);
        const auto& names  = renderer.GetGraphNames();
        const bool  taken  = std::ranges::find(names, name) != names.end();
        const bool  valid  = IsValidGraphName(name) && !taken;
        if (!name.empty() && !valid)
        {
            ImGui::TextColored(PROBLEM_COLOR, taken ? "A graph with that name exists."
                                                    : "Use letters, digits, '_' and '-' only.");
        }

        ImGui::BeginDisabled(!valid);
        if (ImGui::Button("Create"))
        {
            const Renderer::GraphAsset* templateAsset = renderer.FindGraphAsset(TEMPLATE_GRAPH);
            const std::filesystem::path directory     = renderer.GetGraphFile(TEMPLATE_GRAPH).parent_path();
            std::string error;
            if (!templateAsset || directory.empty())
                m_Status = "Cannot create a graph: the 'game' graph it copies is not loaded.";
            else if (!Renderer::WriteGraphAssetFile(*templateAsset, directory / (name + ".graph"), &error))
                m_Status = "Cannot create the graph: " + error;
            else
            {
                m_Status.clear();
                RequestOpenGraph(name); // the renderer registers the file on its next poll
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    void RenderGraphEditorWindow::DrawUnsavedChangesPopup(const Renderer::Renderer& renderer)
    {
        if (m_Pending != PendingAction::None && !ImGui::IsPopupOpen(UNSAVED_POPUP))
            ImGui::OpenPopup(UNSAVED_POPUP);
        if (!ImGui::BeginPopupModal(UNSAVED_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::Text("'%s' has unsaved changes.", m_GraphName.c_str());
        bool proceed = false;
        if (ImGui::Button("Save"))
        {
            Save(renderer);
            proceed = !IsDirty(); // a failed save keeps the prompt's question open
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            m_Edited = m_Saved;
            proceed  = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }

        if (proceed)
        {
            if (m_Pending == PendingAction::OpenGraph)
                OpenGraph(m_PendingGraph);
            else
                Open = false;
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void RenderGraphEditorWindow::RequestOpenGraph(const std::string& name)
    {
        if (name == m_GraphName)
            return;
        if (IsDirty())
        {
            m_Pending      = PendingAction::OpenGraph;
            m_PendingGraph = name;
            return;
        }
        OpenGraph(name);
    }

    void RenderGraphEditorWindow::OpenGraph(const std::string& name)
    {
        m_GraphName = name;
        m_HasGraph  = false;
        m_Model     = {};
        m_Status.clear();
    }

    void RenderGraphEditorWindow::SyncGraph(const Renderer::Renderer& renderer)
    {
        if (m_GraphName.empty())
            return;
        const Renderer::GraphAsset* asset = renderer.FindGraphAsset(m_GraphName);
        if (!asset)
            return; // the toolbar says why

        if (!m_HasGraph)
        {
            m_Library = m_Saved = m_Edited = *asset;
            m_GraphFile = renderer.GetGraphFile(m_GraphName);
            m_Layout    = LoadGraphLayout(GetGraphLayoutFile(m_GraphFile)).value_or(GraphLayout{});
            m_HasGraph  = true;
            m_FitAfterPlacing = true;
            ApplyEdit(true);
            return;
        }

        if (*asset == m_Library)
            return;
        // The renderer reloaded the file: after our own save, or an edit made outside the editor.
        m_Library = *asset;
        if (!IsDirty())
        {
            m_Saved = m_Edited = *asset;
            ApplyEdit(true);
        }
        else if (!(*asset == m_Edited))
        {
            m_Status = "The file changed on disk. Your unsaved edits are kept and overwrite it on save.";
        }
    }

    void RenderGraphEditorWindow::Save(const Renderer::Renderer& renderer)
    {
        (void)renderer;
        std::string error;
        if (!Renderer::WriteGraphAssetFile(m_Edited, m_GraphFile, &error))
        {
            m_Status = "Save failed: " + error;
            return;
        }
        m_Saved  = m_Edited;
        m_Status = "Saved " + m_GraphFile.filename().string() + ".";
        SaveGraphLayout(GetGraphLayoutFile(m_GraphFile), m_Layout);
    }

    // ── Canvas ───────────────────────────────────────────────────────────────

    void RenderGraphEditorWindow::ApplyEdit(bool nodesChanged)
    {
        const Renderer::Renderer* renderer = m_Renderer;
        m_Model = BuildGraphCanvasModel(m_Edited, [renderer](std::string_view type)
        {
            return renderer ? renderer->FindPassType(type) : nullptr;
        });
        // Node, pin and link ids follow the asset's order, so a node added or removed renumbers
        // the ones after it: re-place them all, and drop a selection that now means another node.
        if (nodesChanged)
        {
            m_PlaceNodes     = true;
            m_ClearSelection = true;
        }
        // Keep the layout to the nodes that exist, so a removed node is not saved.
        GraphLayout layout;
        for (const GraphCanvasNode& node : m_Model.Nodes)
        {
            if (const auto it = m_Layout.find(node.Key); it != m_Layout.end())
                layout.emplace(node.Key, it->second);
        }
        m_Layout = std::move(layout);
    }

    void RenderGraphEditorWindow::DrawCanvas(const Renderer::Renderer* renderer)
    {
        ed::SetCurrentEditor(m_Canvas);
        ed::Begin("RenderGraphCanvas");

        if (m_ClearSelection)
        {
            ed::ClearSelection();
            m_ClearSelection = false;
        }
        if (m_PlaceNodes)
        {
            // Saved positions first; a node the layout does not know takes its automatic place.
            for (const GraphCanvasNode& node : m_Model.Nodes)
            {
                const auto saved = m_Layout.find(node.Key);
                const GraphNodePosition position = saved != m_Layout.end() ? saved->second : AutomaticPosition(node);
                m_Layout[node.Key] = position;
                ed::SetNodePosition(ed::NodeId(node.Id), { position.X, position.Y });
            }
            m_PlaceNodes = false;
            if (m_FitAfterPlacing)
            {
                m_FitViewInFrames = FIT_VIEW_DELAY_FRAMES;
                m_FitAfterPlacing = false;
            }
        }

        for (const GraphCanvasNode& node : m_Model.Nodes)
            DrawNode(node);
        for (const GraphCanvasLink& link : m_Model.Links)
            ed::Link(ed::LinkId(link.Id), ed::PinId(link.FromPin), ed::PinId(link.ToPin));

        if (m_HasGraph)
        {
            HandleNewLinks();
            HandleDeletions();
            if (renderer)
                DrawAddMenu(*renderer);
        }

        if (m_FitViewInFrames > 0 && --m_FitViewInFrames == 0)
            ed::NavigateToContent();
        ed::End();

        SaveMovedNodes();
        ed::SetCurrentEditor(nullptr);
    }

    void RenderGraphEditorWindow::DrawNode(const GraphCanvasNode& node) const
    {
        ed::BeginNode(ed::NodeId(node.Id));
        ImGui::TextColored(TitleColor(node), "%s", node.Title.c_str());
        for (const std::string& detail : node.Details)
            ImGui::TextDisabled("%s", detail.c_str());

        for (const GraphCanvasPin& pin : node.Pins)
        {
            ed::BeginPin(ed::PinId(pin.Id), pin.OnRight ? ed::PinKind::Output : ed::PinKind::Input);
            // Links meet a node at its side: an input pin's left edge, an output pin's right edge.
            ed::PinPivotAlignment(pin.OnRight ? ImVec2(1.0f, 0.5f) : ImVec2(0.0f, 0.5f));
            if (pin.Label.empty()) // a resource, import or output node's single pin
                ImGui::TextUnformatted("->");
            else if (pin.Problem)
                ImGui::TextColored(PROBLEM_COLOR, pin.OnRight ? "%s ->" : "-> %s", pin.Label.c_str());
            else
                ImGui::Text(pin.OnRight ? "%s ->" : "-> %s", pin.Label.c_str());
            ed::EndPin();
        }
        ed::EndNode();
    }

    void RenderGraphEditorWindow::DrawAddMenu(const Renderer::Renderer& renderer)
    {
        // Inside the canvas the mouse position is in canvas space: where the new node goes.
        const ImVec2 mouse = ImGui::GetMousePos();
        ed::Suspend();
        if (ed::ShowBackgroundContextMenu())
        {
            m_AddPosition = { std::round(mouse.x), std::round(mouse.y) };
            ImGui::OpenPopup(ADD_MENU_POPUP);
        }

        if (ImGui::BeginPopup(ADD_MENU_POPUP))
        {
            const auto place = [this](GraphNodeKind kind, const std::string& name)
            {
                m_Layout[GetGraphNodeKey(kind, name)] = m_AddPosition;
                ApplyEdit(true);
            };
            if (ImGui::BeginMenu("Pass"))
            {
                for (const std::string& type : renderer.GetPassTypeNames())
                {
                    if (ImGui::MenuItem(type.c_str()))
                        place(GraphNodeKind::Pass, GraphEdit::AddPass(m_Edited, type));
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Resource"))
                place(GraphNodeKind::Resource, GraphEdit::AddResourceLike(m_Edited, GraphNodeKind::Resource, "resource"));
            if (ImGui::MenuItem("Import"))
                place(GraphNodeKind::Import, GraphEdit::AddResourceLike(m_Edited, GraphNodeKind::Import, "import"));
            if (ImGui::MenuItem("Output"))
                place(GraphNodeKind::Output, GraphEdit::AddResourceLike(m_Edited, GraphNodeKind::Output, "output"));
            ImGui::EndPopup();
        }
        ed::Resume();
    }

    void RenderGraphEditorWindow::HandleNewLinks()
    {
        if (ed::BeginCreate(RESOURCE_COLOR, 2.0f))
        {
            ed::PinId startId;
            ed::PinId endId;
            if (ed::QueryNewLink(&startId, &endId) && startId && endId)
            {
                const GraphCanvasNode* startNode = nullptr;
                const GraphCanvasNode* endNode   = nullptr;
                const GraphCanvasPin*  startPin  = FindPin(static_cast<uint32_t>(startId.Get()), &startNode);
                const GraphCanvasPin*  endPin    = FindPin(static_cast<uint32_t>(endId.Get()), &endNode);

                // A binding joins a pass's slot pin and a resource, import or output: either way round.
                const bool startIsPass = startNode && startNode->Kind == GraphNodeKind::Pass;
                const bool endIsPass   = endNode && endNode->Kind == GraphNodeKind::Pass;
                const GraphCanvasNode* passNode     = startIsPass ? startNode : endNode;
                const GraphCanvasPin*  slotPin      = startIsPass ? startPin : endPin;
                const GraphCanvasNode* resourceNode = startIsPass ? endNode : startNode;

                if (startPin && endPin && startIsPass != endIsPass)
                {
                    if (ed::AcceptNewItem(OUTPUT_COLOR, 2.0f))
                    {
                        GraphEdit::BindSlot(m_Edited, passNode->Index, slotPin->Slot, resourceNode->Name);
                        ApplyEdit(false);
                    }
                }
                else
                {
                    ed::RejectNewItem(PROBLEM_COLOR, 2.0f);
                }
            }
        }
        ed::EndCreate();
    }

    void RenderGraphEditorWindow::HandleDeletions()
    {
        struct Removal
        {
            GraphNodeKind Kind;
            size_t        Index;
        };
        std::vector<std::pair<size_t, std::string>> unbinds;
        std::vector<Removal>                        removals;

        if (ed::BeginDelete())
        {
            ed::LinkId linkId;
            while (ed::QueryDeletedLink(&linkId))
            {
                const auto link = std::ranges::find(m_Model.Links, static_cast<uint32_t>(linkId.Get()), &GraphCanvasLink::Id);
                if (link != m_Model.Links.end() && ed::AcceptDeletedItem())
                    unbinds.emplace_back(link->PassIndex, link->Slot);
            }
            ed::NodeId nodeId;
            while (ed::QueryDeletedNode(&nodeId))
            {
                const GraphCanvasNode* node = FindNode(static_cast<uint32_t>(nodeId.Get()));
                if (node && ed::AcceptDeletedItem())
                    removals.push_back({ node->Kind, node->Index });
            }
        }
        ed::EndDelete();

        if (unbinds.empty() && removals.empty())
            return;

        // Unbind first, while the pass indices are still valid; then remove from the back, so an
        // earlier removal never shifts a later one.
        for (const auto& [passIndex, slot] : unbinds)
            GraphEdit::UnbindSlot(m_Edited, passIndex, slot);
        std::ranges::sort(removals, [](const Removal& a, const Removal& b) { return a.Index > b.Index; });
        for (const Removal& removal : removals)
        {
            if (removal.Kind == GraphNodeKind::Pass)
                GraphEdit::RemovePass(m_Edited, removal.Index);
            else
                GraphEdit::RemoveResourceLike(m_Edited, removal.Kind, removal.Index);
        }
        ApplyEdit(!removals.empty());
    }

    void RenderGraphEditorWindow::SaveMovedNodes()
    {
        // After an edit this frame the model has new ids that the canvas has not been told the
        // positions of yet: reading positions back now would give each node another's place.
        if (!m_HasGraph || m_PlaceNodes || ImGui::IsMouseDown(ImGuiMouseButton_Left) || m_GraphFile.empty())
            return;

        bool moved = false;
        for (const GraphCanvasNode& node : m_Model.Nodes)
        {
            const ImVec2            now = ed::GetNodePosition(ed::NodeId(node.Id));
            const GraphNodePosition position{ std::round(now.x), std::round(now.y) };
            GraphNodePosition&      saved = m_Layout[node.Key];
            if (std::isfinite(now.x) && !(position == saved))
            {
                saved = position;
                moved = true;
            }
        }
        if (moved)
            SaveGraphLayout(GetGraphLayoutFile(m_GraphFile), m_Layout);
    }

    const GraphCanvasNode* RenderGraphEditorWindow::FindNode(uint32_t id) const
    {
        const auto it = std::ranges::find(m_Model.Nodes, id, &GraphCanvasNode::Id);
        return it != m_Model.Nodes.end() ? &*it : nullptr;
    }

    const GraphCanvasPin* RenderGraphEditorWindow::FindPin(uint32_t id, const GraphCanvasNode** owner) const
    {
        for (const GraphCanvasNode& node : m_Model.Nodes)
        {
            for (const GraphCanvasPin& pin : node.Pins)
            {
                if (pin.Id == id)
                {
                    *owner = &node;
                    return &pin;
                }
            }
        }
        return nullptr;
    }
}
