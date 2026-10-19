#pragma once

#include "ControllerLayout.hpp"

#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
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

namespace Editor
{
    // What the editor's selection tells the window: the clips the selected entity's mesh has (for
    // the state's clip combo) and, when its animator runs a controller, which one and the state it
    // is in, so the window can follow it during Play.
    struct AnimatorPreview
    {
        std::vector<std::string> Clips;
        std::string              ControllerKey; // FS::MakeAssetKey of the animator's controller
        std::string              ActiveState;   // empty when no controller runs
    };

    // The animator controller editor (Window > Animator Controller): a .animctrl file drawn as nodes
    // with imgui-node-editor, and edited there through ControllerEditing. States are nodes, with an
    // Entry node leading to the default state and an Any State node; transitions are links, from a
    // state's right pin to another's left pin.
    //
    // Editing: right-click the canvas to add a state, or a state to make it the default or delete
    // it; drag from a state's (or Any State's) right pin to a state to add a transition, or from
    // Entry to make that state the default; select a link or node and press Delete to remove it.
    // The side panel edits the parameters and the selected state or transition.
    //
    // Files: New starts an empty, untitled controller; Open... picks any .animctrl under a mount.
    // Save (Ctrl+S) writes it, or asks where for an untitled one; Save As... writes it to a new
    // file and goes on editing that. The engine reloads a saved file within a second, so a running
    // animator picks it up in Play. Node positions are kept in "<name>.animctrl.layout" beside it.
    //
    // Every edit is checked with ValidateAnimatorController (with the selected mesh's clips when
    // its animator runs this controller): states and transitions at fault are drawn red, and a list
    // under the canvas names every problem. During Play the state the selected animator is in is
    // outlined, and the transition that led there flows.
    class AnimatorControllerEditorWindow
    {
    public:
        AnimatorControllerEditorWindow() = default;
        ~AnimatorControllerEditorWindow();

        AnimatorControllerEditorWindow(const AnimatorControllerEditorWindow&)            = delete;
        AnimatorControllerEditorWindow& operator=(const AnimatorControllerEditorWindow&) = delete;
        AnimatorControllerEditorWindow(AnimatorControllerEditorWindow&&)                 = delete;
        AnimatorControllerEditorWindow& operator=(AnimatorControllerEditorWindow&&)      = delete;

        bool Open = false;

        void Draw(const FS::FileSystemManager& fileSystem, const AnimatorPreview& preview);

        // Shows the window on the controller at virtualPath, asking first when there are unsaved edits.
        void OpenController(const std::string& virtualPath);

    private:
        enum class PendingAction
        {
            None,
            OpenController, // switch to m_PendingPath
            NewController,
            Close,
        };

        // ── File lifetime ────────────────────────────────────────────────────
        void DrawToolbar(const FS::FileSystemManager& fileSystem);
        void DrawUnsavedChangesPopup(const FS::FileSystemManager& fileSystem);
        void RequestOpen(const std::string& virtualPath);
        void RequestNew();
        // Reads the file at virtualPath and its layout; on failure the status says why.
        void Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
        void NewController();
        void OpenFile(const FS::FileSystemManager& fileSystem);
        void Save(const FS::FileSystemManager& fileSystem);
        void SaveAs(const FS::FileSystemManager& fileSystem);
        bool Write(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
        bool IsUntitled() const { return m_HasController && m_Path.empty(); }
        bool IsDirty() const { return m_HasController && m_EditedText != m_SavedText; }

        // ── Canvas ───────────────────────────────────────────────────────────
        void DrawCanvas(const AnimatorPreview& preview);
        void DrawStateNode(uint32_t state, bool active) const;
        void DrawContextMenus(const AnimatorPreview& preview);
        void HandleNewLinks();
        void HandleDeletions();
        void ReadSelection();
        void FollowActiveState(const AnimatorPreview& preview);
        void SaveMovedNodes();
        void DrawProblems();

        // ── Side panel (AnimatorControllerDetailsPanel.cpp) ──────────────────
        void DrawDetails(const AnimatorPreview& preview);
        void DrawParameters();
        void DrawStateDetails(uint32_t state, const AnimatorPreview& preview);
        void DrawTransitionDetails(uint32_t transition);

        // After an edit: writes the text the dirty check compares, validates, and keeps the layout
        // to the states that exist. Pass statesChanged when a state was added, removed or renamed,
        // which renumbers or rekeys the nodes.
        void ApplyEdit(bool statesChanged);
        void Validate();
        bool UsesThisController(const AnimatorPreview& preview) const;

        ax::NodeEditor::EditorContext* m_Canvas = nullptr;

        std::string                           m_Path; // virtual; empty for an untitled controller
        std::filesystem::path                 m_File; // physical, for the layout file
        bool                                  m_HasController = false;
        HedgehogAnimation::AnimatorController m_Edited;
        std::string                           m_EditedText; // m_Edited as written, to compare
        std::string                           m_SavedText;  // what the file holds
        GraphLayout                           m_Layout;
        std::string                           m_Status;

        std::vector<std::string>                  m_Problems;
        std::unordered_map<uint32_t, std::string> m_StateProblems;      // state index to its problems
        std::unordered_map<uint32_t, std::string> m_TransitionProblems; // transition index to its problems
        std::vector<std::string>                  m_ValidatedClips;     // the clips m_Problems were checked with

        std::optional<uint32_t> m_SelectedState;
        std::optional<uint32_t> m_SelectedTransition;
        std::string             m_LastActiveState; // to notice the selected animator change state
        // Selected on the next canvas frame, once ids an edit renumbered are drawn.
        std::optional<uint32_t> m_FocusState;
        std::optional<uint32_t> m_FocusTransition;
        uint32_t                m_ContextState = 0; // the state whose context menu is open

        bool m_PlaceNodes      = false;
        bool m_ClearSelection  = false;
        int  m_FitViewInFrames = 0;

        PendingAction     m_Pending = PendingAction::None;
        std::string       m_PendingPath;
        std::string       m_LoadPath; // read on the next Draw, which has the file system
        GraphNodePosition m_AddPosition;
    };
}
