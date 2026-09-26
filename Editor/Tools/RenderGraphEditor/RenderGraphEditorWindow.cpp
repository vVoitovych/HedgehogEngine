#include "RenderGraphEditorWindow.hpp"

#include "HedgehogRenderer/Renderer.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <cmath>

namespace ed = ax::NodeEditor;

namespace Editor
{
    namespace
    {
        constexpr float COLUMN_WIDTH = 300.0f;
        constexpr float ROW_HEIGHT   = 150.0f;

        constexpr int FIT_VIEW_DELAY_FRAMES = 10;

        constexpr ImVec4 PASS_COLOR     = { 1.00f, 1.00f, 1.00f, 1.0f };
        constexpr ImVec4 RESOURCE_COLOR = { 0.55f, 0.80f, 1.00f, 1.0f };
        constexpr ImVec4 IMPORT_COLOR   = { 0.80f, 0.65f, 1.00f, 1.0f };
        constexpr ImVec4 OUTPUT_COLOR   = { 0.55f, 0.95f, 0.55f, 1.0f };
        constexpr ImVec4 PROBLEM_COLOR  = { 0.95f, 0.35f, 0.35f, 1.0f };

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

        if (!m_Canvas)
        {
            // No settings file: node positions live in the graph's own layout file instead.
            ed::Config config;
            config.SettingsFile = nullptr;
            m_Canvas = ed::CreateEditor(&config);
        }

        ImGui::SetNextWindowSize({ 960.0f, 600.0f }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Render Graph Editor", &Open))
        {
            if (renderer)
            {
                DrawGraphPicker(*renderer);
                SyncGraph(*renderer);
            }
            ImGui::TextDisabled("Read-only. Drag nodes to arrange them; right-drag pans, the wheel zooms.");
            DrawCanvas();
        }
        ImGui::End();
    }

    void RenderGraphEditorWindow::DrawGraphPicker(const Renderer::Renderer& renderer)
    {
        ImGui::SetNextItemWidth(240.0f);
        if (!ImGui::BeginCombo("Graph", m_GraphName.empty() ? "(open a graph)" : m_GraphName.c_str()))
            return;
        for (const std::string& name : renderer.GetGraphNames())
        {
            const bool chosen = name == m_GraphName;
            if (ImGui::Selectable(name.c_str(), chosen) && !chosen)
            {
                m_GraphName = name;
                m_HasGraph  = false;
            }
            if (chosen)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    void RenderGraphEditorWindow::SyncGraph(const Renderer::Renderer& renderer)
    {
        if (m_GraphName.empty())
            return;

        const Renderer::GraphAsset* asset = renderer.FindGraphAsset(m_GraphName);
        if (!asset)
        {
            ImGui::TextColored(PROBLEM_COLOR, "'%s' has never loaded; the log names the error.", m_GraphName.c_str());
            m_HasGraph = false;
            m_Model    = {};
            return;
        }
        if (m_HasGraph && *asset == m_Asset)
            return;

        const bool reloaded = m_HasGraph; // same graph, new contents: keep the view where it is
        m_Asset      = *asset;
        m_HasGraph   = true;
        m_Model      = BuildGraphCanvasModel(m_Asset, [&renderer](std::string_view type) { return renderer.FindPassType(type); });
        if (!reloaded)
        {
            m_LayoutFile = GetGraphLayoutFile(renderer.GetGraphFile(m_GraphName));
            m_Layout     = LoadGraphLayout(m_LayoutFile).value_or(GraphLayout{});
        }
        m_PlaceNodes = true;
    }

    void RenderGraphEditorWindow::DrawCanvas()
    {
        ed::SetCurrentEditor(m_Canvas);
        ed::Begin("RenderGraphCanvas");

        if (m_PlaceNodes)
        {
            // Saved positions first; a node the layout file does not know takes its automatic
            // place, and is saved with the others the next time a node is moved.
            for (const GraphCanvasNode& node : m_Model.Nodes)
            {
                const auto saved = m_Layout.find(node.Key);
                const GraphNodePosition position = saved != m_Layout.end() ? saved->second : AutomaticPosition(node);
                m_Layout[node.Key] = position;
                ed::SetNodePosition(ed::NodeId(node.Id), { position.X, position.Y });
            }
            m_PlaceNodes = false;
            // Fit the view to the nodes a few frames later: on this frame they have no size yet, and
            // a window that has just opened takes a few frames to settle its own size, so fitting
            // sooner is silently undone.
            m_FitViewInFrames = FIT_VIEW_DELAY_FRAMES;
        }

        for (const GraphCanvasNode& node : m_Model.Nodes)
            DrawNode(node);
        for (const GraphCanvasLink& link : m_Model.Links)
            ed::Link(ed::LinkId(link.Id), ed::PinId(link.FromPin), ed::PinId(link.ToPin));

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

    void RenderGraphEditorWindow::SaveMovedNodes()
    {
        if (!m_HasGraph || ImGui::IsMouseDown(ImGuiMouseButton_Left) || m_LayoutFile.empty())
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
            SaveGraphLayout(m_LayoutFile, m_Layout);
    }
}
