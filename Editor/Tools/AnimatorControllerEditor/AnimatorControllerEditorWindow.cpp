#include "AnimatorControllerEditorWindow.hpp"

#include "ControllerEditing.hpp"
#include "Tools/RenderGraphEditor/RenderGraphEditorStyle.hpp"

#include "DialogueWindows/api/AnimatorControllerDialogue.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "HedgehogAnimation/api/AnimatorControllerFile.hpp"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ed = ax::NodeEditor;

namespace Editor
{
    namespace
    {
        using HedgehogAnimation::ANY_STATE;

        constexpr float COLUMN_WIDTH         = 220.0f;
        constexpr float ROW_HEIGHT           = 130.0f;
        constexpr int   STATES_PER_ROW       = 4;
        constexpr float PROBLEM_BORDER_WIDTH = 2.5f;
        constexpr float LINK_THICKNESS       = 2.0f;
        constexpr size_t MAX_LISTED_ROWS     = 4;
        // As the render graph editor: a new window settles its size before fitting the view sticks.
        constexpr int FIT_VIEW_DELAY_FRAMES = 10;

        constexpr ImVec4 ENTRY_COLOR = { 0.55f, 0.95f, 0.55f, 1.0f };
        constexpr ImVec4 ANY_COLOR   = { 0.80f, 0.65f, 1.00f, 1.0f };
        constexpr ImVec4 LINK_COLOR  = { 0.85f, 0.85f, 0.85f, 1.0f };

        constexpr const char* UNSAVED_POPUP   = "Unsaved controller changes";
        constexpr const char* UNTITLED        = "untitled";
        constexpr const char* DIALOGUE_FOLDER = "assets://";

        // Canvas ids. Entry and Any State are fixed; state i is node STATE_NODE + i with pins
        // STATE_PIN + 2i (in, on the left) and + 1 (out, on the right); transition t is link
        // TRANSITION_LINK + t, so an edit that renumbers them renumbers the ids.
        constexpr uint64_t ENTRY_NODE      = 1;
        constexpr uint64_t ANY_NODE        = 2;
        constexpr uint64_t ENTRY_PIN       = 3;
        constexpr uint64_t ANY_PIN         = 4;
        constexpr uint64_t ENTRY_LINK      = 5;
        constexpr uint64_t STATE_NODE      = 1u << 16;
        constexpr uint64_t STATE_PIN       = 1u << 20;
        constexpr uint64_t TRANSITION_LINK = 1u << 24;

        uint64_t StateInPin(uint32_t state) { return STATE_PIN + 2u * state; }
        uint64_t StateOutPin(uint32_t state) { return STATE_PIN + 2u * state + 1u; }

        GraphNodePosition AutomaticStatePosition(uint32_t state)
        {
            return { COLUMN_WIDTH * static_cast<float>(1 + state % STATES_PER_ROW),
                     ROW_HEIGHT * static_cast<float>(state / STATES_PER_ROW) };
        }

        void DrawPinArrow(uint64_t pin, bool onRight)
        {
            ed::BeginPin(ed::PinId(pin), onRight ? ed::PinKind::Output : ed::PinKind::Input);
            ed::PinPivotAlignment(onRight ? ImVec2(1.0f, 0.5f) : ImVec2(0.0f, 0.5f));
            ImGui::TextUnformatted("->");
            ed::EndPin();
        }

        void DrawFixedNode(uint64_t node, uint64_t pin, const char* title, const ImVec4& color)
        {
            ed::BeginNode(ed::NodeId(node));
            ImGui::TextColored(color, "%s", title);
            DrawPinArrow(pin, true);
            ed::EndNode();
        }
    }

    AnimatorControllerEditorWindow::~AnimatorControllerEditorWindow()
    {
        if (m_Canvas)
            ed::DestroyEditor(m_Canvas);
    }

    void AnimatorControllerEditorWindow::Draw(const FS::FileSystemManager& fileSystem, const AnimatorPreview& preview)
    {
        if (!Open)
            return;
        if (!m_Canvas)
        {
            ed::Config config;
            config.SettingsFile = nullptr; // positions live in the controller's layout file
            m_Canvas = ed::CreateEditor(&config);
            ed::SetCurrentEditor(m_Canvas);
            ApplyCanvasTheme(ed::GetStyle());
            ed::SetCurrentEditor(nullptr);
        }
        if (!m_LoadPath.empty())
        {
            Load(m_LoadPath, fileSystem);
            m_LoadPath.clear();
        }
        // The mesh's clips check the states' clips only while the selection runs this controller.
        const std::vector<std::string> clips = UsesThisController(preview) ? preview.Clips : std::vector<std::string>{};
        if (m_HasController && clips != m_ValidatedClips)
        {
            m_ValidatedClips = clips;
            Validate();
        }

        std::string title = "Animator Controller";
        if (IsUntitled())
            title += std::string(" - ") + UNTITLED;
        else if (!m_Path.empty())
            title += " - " + std::filesystem::path(m_Path).filename().string();
        title += IsDirty() ? "*" : "";
        title += "###AnimatorControllerEditor";

        bool keepOpen = true;
        ImGui::SetNextWindowSize({ 1100.0f, 600.0f }, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title.c_str(), &keepOpen))
        {
            DrawToolbar(fileSystem);
            const float problemsHeight = m_Problems.empty() ? 0.0f
                : ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(std::min(m_Problems.size(), MAX_LISTED_ROWS) + 1)
                  + ImGui::GetStyle().WindowPadding.y * 2.0f;
            ImGui::BeginChild("ControllerCanvasRegion", ImVec2(0.0f, -problemsHeight));
            DrawCanvas();
            ImGui::EndChild();
            if (!m_Problems.empty())
            {
                ImGui::BeginChild("ControllerProblems", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                DrawProblems();
                ImGui::EndChild();
            }

            if (IsDirty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
            {
                Save(fileSystem);
            }
            DrawUnsavedChangesPopup(fileSystem);
        }
        ImGui::End();

        if (!keepOpen)
        {
            if (IsDirty())
                m_Pending = PendingAction::Close;
            else
                Open = false;
        }
    }

    void AnimatorControllerEditorWindow::OpenController(const std::string& virtualPath)
    {
        Open = true;
        RequestOpen(virtualPath);
    }

    bool AnimatorControllerEditorWindow::UsesThisController(const AnimatorPreview& preview) const
    {
        return !m_Path.empty() && !preview.ControllerKey.empty() && preview.ControllerKey == FS::MakeAssetKey(m_Path);
    }

    // ── File lifetime ────────────────────────────────────────────────────────

    void AnimatorControllerEditorWindow::DrawToolbar(const FS::FileSystemManager& fileSystem)
    {
        if (ImGui::Button("New"))
            RequestNew();
        ImGui::SameLine();
        if (ImGui::Button("Open..."))
            OpenFile(fileSystem);
        ImGui::SameLine();
        ImGui::BeginDisabled(!IsDirty() && !IsUntitled());
        if (ImGui::Button("Save"))
            Save(fileSystem);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_HasController);
        if (ImGui::Button("Save As..."))
            SaveAs(fileSystem);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(IsUntitled() ? UNTITLED : m_Path.empty() ? "(open a controller)" : m_Path.c_str());

        if (!m_Status.empty())
            ImGui::TextUnformatted(m_Status.c_str());
        else
            ImGui::TextDisabled("Drag the states to arrange them; their places are saved beside the file.");
    }

    void AnimatorControllerEditorWindow::DrawUnsavedChangesPopup(const FS::FileSystemManager& fileSystem)
    {
        if (m_Pending != PendingAction::None && !ImGui::IsPopupOpen(UNSAVED_POPUP))
            ImGui::OpenPopup(UNSAVED_POPUP);
        if (!ImGui::BeginPopupModal(UNSAVED_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::Text("'%s' has unsaved changes.", IsUntitled() ? UNTITLED : m_Path.c_str());
        bool proceed = false;
        if (ImGui::Button("Save"))
        {
            Save(fileSystem);
            proceed = !IsDirty(); // a failed save keeps the question open
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            // Read the file again, so reopening the window shows what it holds; an untitled
            // controller is dropped.
            m_EditedText    = m_SavedText;
            m_LoadPath      = m_Path;
            m_HasController = !m_Path.empty();
            proceed         = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }

        if (proceed)
        {
            if (m_Pending == PendingAction::OpenController)
                m_LoadPath = m_PendingPath;
            else if (m_Pending == PendingAction::NewController)
                NewController();
            else
                Open = false;
            m_Pending = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void AnimatorControllerEditorWindow::RequestOpen(const std::string& virtualPath)
    {
        if (m_HasController && FS::MakeAssetKey(virtualPath) == FS::MakeAssetKey(m_Path))
            return;
        if (IsDirty())
        {
            m_Pending     = PendingAction::OpenController;
            m_PendingPath = virtualPath;
            return;
        }
        m_LoadPath = virtualPath;
    }

    void AnimatorControllerEditorWindow::RequestNew()
    {
        if (IsDirty())
            m_Pending = PendingAction::NewController;
        else
            NewController();
    }

    void AnimatorControllerEditorWindow::Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        const std::string path = FS::ToAssetVirtualPath(virtualPath);
        const std::optional<std::string> text = fileSystem.ReadTextFile(path);
        if (!text)
        {
            m_Status = path + " cannot be read.";
            return;
        }
        HedgehogAnimation::AnimatorControllerParseResult parsed = HedgehogAnimation::ParseAnimatorController(*text);
        if (!parsed.Controller)
        {
            m_Status = path + " cannot be opened: " + parsed.Error;
            return;
        }
        m_Edited        = std::move(*parsed.Controller);
        m_Path          = path;
        m_File          = fileSystem.ResolvePhysical(path).value_or(std::filesystem::path{});
        m_Layout        = m_File.empty() ? GraphLayout{} : LoadGraphLayout(GetGraphLayoutFile(m_File)).value_or(GraphLayout{});
        m_HasController = true;
        m_Status.clear();
        ApplyEdit(true);
        m_SavedText       = m_EditedText;
        m_FitViewInFrames = FIT_VIEW_DELAY_FRAMES;
    }

    void AnimatorControllerEditorWindow::NewController()
    {
        m_Edited = {};
        m_Path.clear();
        m_File.clear();
        m_Layout        = {};
        m_HasController = true;
        m_Status.clear();
        ApplyEdit(true);
        m_SavedText = m_EditedText;
    }

    void AnimatorControllerEditorWindow::OpenFile(const FS::FileSystemManager& fileSystem)
    {
        const std::optional<std::filesystem::path> folder = fileSystem.ResolvePhysical(DIALOGUE_FOLDER);
        const std::string start = folder ? std::filesystem::path(*folder).make_preferred().string() + "\\" : std::string();
        const char* picked = DialogueWindows::AnimatorControllerOpenDialogue(start.c_str());
        if (!picked)
            return;
        const std::optional<std::string> virtualPath = fileSystem.ToVirtualPath(picked);
        if (!virtualPath)
        {
            m_Status = std::string(picked) + " is not under a mounted folder, so no animator can use it.";
            return;
        }
        RequestOpen(*virtualPath);
    }

    void AnimatorControllerEditorWindow::Save(const FS::FileSystemManager& fileSystem)
    {
        if (m_Path.empty())
            SaveAs(fileSystem);
        else if (Write(m_Path, fileSystem))
            m_SavedText = m_EditedText;
    }

    void AnimatorControllerEditorWindow::SaveAs(const FS::FileSystemManager& fileSystem)
    {
        std::string start = m_File.string();
        if (start.empty())
        {
            const std::optional<std::filesystem::path> folder = fileSystem.ResolvePhysical(DIALOGUE_FOLDER);
            start = folder ? (*folder / "Untitled.animctrl").make_preferred().string() : "Untitled.animctrl";
        }
        const char* picked = DialogueWindows::AnimatorControllerSaveDialogue(start.c_str());
        if (!picked)
            return;
        std::filesystem::path file(picked);
        if (file.extension() != HedgehogAnimation::ANIMATOR_CONTROLLER_EXTENSION)
            file += HedgehogAnimation::ANIMATOR_CONTROLLER_EXTENSION;
        const std::optional<std::string> virtualPath = fileSystem.ToVirtualPath(file);
        if (!virtualPath)
        {
            m_Status = file.string() + " is not under a mounted folder, so no animator could use it.";
            return;
        }
        if (!Write(*virtualPath, fileSystem))
            return;
        m_Path      = *virtualPath;
        m_File      = file;
        m_SavedText = m_EditedText;
        SaveGraphLayout(GetGraphLayoutFile(m_File), m_Layout);
    }

    bool AnimatorControllerEditorWindow::Write(const std::string& virtualPath, const FS::FileSystemManager& fileSystem)
    {
        if (!fileSystem.WriteTextFile(virtualPath, m_EditedText))
        {
            m_Status = "Save failed: " + virtualPath + " cannot be written.";
            return false;
        }
        m_Status = "Saved " + virtualPath + ".";
        if (!m_Problems.empty())
        {
            m_Status += " It has " + std::to_string(m_Problems.size())
                      + " problem(s), so animators using it play their Clip until they are fixed.";
        }
        if (!m_File.empty())
            SaveGraphLayout(GetGraphLayoutFile(m_File), m_Layout);
        return true;
    }

    // ── Canvas ───────────────────────────────────────────────────────────────

    void AnimatorControllerEditorWindow::ApplyEdit(bool statesChanged)
    {
        m_EditedText = HedgehogAnimation::WriteAnimatorController(m_Edited);
        Validate();
        if (statesChanged)
        {
            m_PlaceNodes     = true;
            m_ClearSelection = true;
        }

        // Keep the layout to the nodes that exist, so a deleted state's place is not saved.
        GraphLayout layout;
        const auto keep = [&](const std::string& key)
        {
            if (const auto it = m_Layout.find(key); it != m_Layout.end())
                layout.emplace(key, it->second);
        };
        keep(std::string(ENTRY_NODE_KEY));
        keep(std::string(ANY_STATE_NODE_KEY));
        for (const HedgehogAnimation::AnimatorState& state : m_Edited.States)
            keep(MakeStateNodeKey(state.Name));
        m_Layout = std::move(layout);
    }

    void AnimatorControllerEditorWindow::Validate()
    {
        m_Problems = HedgehogAnimation::ValidateAnimatorController(m_Edited, m_ValidatedClips.empty() ? nullptr : &m_ValidatedClips);
        m_StateProblems.clear();
        m_TransitionProblems.clear();
        const auto add = [](std::unordered_map<uint32_t, std::string>& map, uint32_t index, const std::string& problem)
        {
            std::string& problems = map[index];
            problems += problems.empty() ? problem : "\n" + problem;
        };
        // The validator names a state as "state '<name>'" and a transition as "transition <n>".
        for (const std::string& problem : m_Problems)
        {
            for (uint32_t s = 0; s < m_Edited.States.size(); ++s)
            {
                if (problem.starts_with("state '" + m_Edited.States[s].Name + "'"))
                    add(m_StateProblems, s, problem);
            }
            constexpr std::string_view TRANSITION = "transition ";
            if (problem.starts_with(TRANSITION))
                add(m_TransitionProblems, static_cast<uint32_t>(std::strtoul(problem.c_str() + TRANSITION.size(), nullptr, 10)), problem);
        }
    }

    void AnimatorControllerEditorWindow::DrawCanvas()
    {
        ed::SetCurrentEditor(m_Canvas);
        ed::Begin("AnimatorControllerCanvas");

        if (m_ClearSelection)
        {
            ed::ClearSelection();
            m_ClearSelection = false;
        }
        if (m_PlaceNodes)
        {
            const auto place = [this](uint64_t node, const std::string& key, GraphNodePosition automatic)
            {
                const auto saved = m_Layout.find(key);
                const GraphNodePosition position = saved != m_Layout.end() ? saved->second : automatic;
                m_Layout[key] = position;
                ed::SetNodePosition(ed::NodeId(node), { position.X, position.Y });
            };
            place(ENTRY_NODE, std::string(ENTRY_NODE_KEY), { 0.0f, 0.0f });
            place(ANY_NODE, std::string(ANY_STATE_NODE_KEY), { 0.0f, ROW_HEIGHT });
            for (uint32_t s = 0; s < m_Edited.States.size(); ++s)
                place(STATE_NODE + s, MakeStateNodeKey(m_Edited.States[s].Name), AutomaticStatePosition(s));
            m_PlaceNodes = false;
        }

        if (m_HasController)
        {
            DrawFixedNode(ENTRY_NODE, ENTRY_PIN, "Entry", ENTRY_COLOR);
            DrawFixedNode(ANY_NODE, ANY_PIN, "Any State", ANY_COLOR);
            for (uint32_t s = 0; s < m_Edited.States.size(); ++s)
                DrawStateNode(s);

            const size_t stateCount = m_Edited.States.size();
            if (m_Edited.DefaultState < stateCount)
                ed::Link(ed::LinkId(ENTRY_LINK), ed::PinId(ENTRY_PIN), ed::PinId(StateInPin(m_Edited.DefaultState)), ENTRY_COLOR, LINK_THICKNESS);
            for (uint32_t t = 0; t < m_Edited.Transitions.size(); ++t)
            {
                const HedgehogAnimation::AnimatorTransition& transition = m_Edited.Transitions[t];
                const bool fromValid = transition.From == ANY_STATE || transition.From < stateCount;
                if (!fromValid || transition.To >= stateCount)
                    continue;
                const uint64_t from = transition.From == ANY_STATE ? ANY_PIN : StateOutPin(transition.From);
                ed::Link(ed::LinkId(TRANSITION_LINK + t), ed::PinId(from), ed::PinId(StateInPin(transition.To)),
                         m_TransitionProblems.contains(t) ? PROBLEM_COLOR : LINK_COLOR, LINK_THICKNESS);
            }

            if (const ed::NodeId hovered = ed::GetHoveredNode(); hovered && hovered.Get() >= STATE_NODE)
            {
                if (const auto problems = m_StateProblems.find(static_cast<uint32_t>(hovered.Get() - STATE_NODE)); problems != m_StateProblems.end())
                {
                    ed::Suspend();
                    ImGui::SetTooltip("%s", problems->second.c_str());
                    ed::Resume();
                }
            }
        }

        if (m_FitViewInFrames > 0 && --m_FitViewInFrames == 0)
            ed::NavigateToContent();
        ed::End();
        SaveMovedNodes();
        ed::SetCurrentEditor(nullptr);
    }

    void AnimatorControllerEditorWindow::DrawStateNode(uint32_t state) const
    {
        const HedgehogAnimation::AnimatorState& data = m_Edited.States[state];
        const bool hasProblem = m_StateProblems.contains(state);
        if (hasProblem)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBorder, PROBLEM_COLOR);
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, PROBLEM_BORDER_WIDTH);
        }
        ed::BeginNode(ed::NodeId(STATE_NODE + state));
        ImGui::TextColored(hasProblem ? PROBLEM_COLOR : PASS_COLOR, "%s", data.Name.c_str());
        if (state == m_Edited.DefaultState)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(default)");
        }
        ImGui::TextDisabled("%s", data.Clip.c_str());
        DrawPinArrow(StateInPin(state), false);
        ImGui::SameLine(0.0f, 60.0f);
        DrawPinArrow(StateOutPin(state), true);
        ed::EndNode();
        if (hasProblem)
        {
            ed::PopStyleVar();
            ed::PopStyleColor();
        }
    }

    void AnimatorControllerEditorWindow::SaveMovedNodes()
    {
        // After an edit this frame the canvas has not been told the renumbered nodes' places yet.
        if (!m_HasController || m_PlaceNodes || ImGui::IsMouseDown(ImGuiMouseButton_Left))
            return;
        bool moved = false;
        const auto read = [&](uint64_t node, const std::string& key)
        {
            const ImVec2            now = ed::GetNodePosition(ed::NodeId(node));
            const GraphNodePosition position{ std::round(now.x), std::round(now.y) };
            GraphNodePosition&      saved = m_Layout[key];
            if (std::isfinite(now.x) && !(position == saved))
            {
                saved = position;
                moved = true;
            }
        };
        read(ENTRY_NODE, std::string(ENTRY_NODE_KEY));
        read(ANY_NODE, std::string(ANY_STATE_NODE_KEY));
        for (uint32_t s = 0; s < m_Edited.States.size(); ++s)
            read(STATE_NODE + s, MakeStateNodeKey(m_Edited.States[s].Name));
        // An untitled controller keeps its places in memory until it is saved.
        if (moved && !m_File.empty())
            SaveGraphLayout(GetGraphLayoutFile(m_File), m_Layout);
    }

    void AnimatorControllerEditorWindow::DrawProblems()
    {
        ImGui::TextColored(PROBLEM_COLOR, "%zu problem(s): animators using this controller play their Clip instead.",
                           m_Problems.size());
        for (const std::string& problem : m_Problems)
            ImGui::BulletText("%s", problem.c_str());
    }
}
