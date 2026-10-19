#pragma once

#include "ControllerLayout.hpp"

#include "HedgehogAnimation/api/AnimatorController.hpp"

#include <cstdint>
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

namespace Editor
{
    // What the editor's selection tells the window: the clips the selected entity's mesh has and,
    // when it has an animator with a controller, which one, so the window checks the states' clips
    // against the mesh that runs it.
    struct AnimatorPreview
    {
        std::vector<std::string> Clips;
        std::string              ControllerKey; // FS::MakeAssetKey of the animator's controller
    };

    // The animator controller editor (Window > Animator Controller): a .animctrl file drawn as nodes
    // with imgui-node-editor. States are nodes, with an Entry node leading to the default state and
    // an Any State node; transitions are links, from a state's right pin to another's left pin.
    //
    // Files: New starts an empty, untitled controller; Open... picks any .animctrl under a mount.
    // Save (Ctrl+S) writes it, or asks where for an untitled one; Save As... writes it to a new
    // file and goes on editing that. The engine reloads a saved file within a second, so a running
    // animator picks it up in Play. Node positions are kept in "<name>.animctrl.layout" beside it.
    //
    // Every edit is checked with ValidateAnimatorController (with the selected mesh's clips when
    // its animator runs this controller): states and transitions at fault are drawn red, and a list
    // under the canvas names every problem.
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
        void DrawCanvas();
        void DrawStateNode(uint32_t state) const;
        void SaveMovedNodes();
        void DrawProblems();

        // After the controller changes: writes the text the dirty check compares, validates, and
        // keeps the layout to the states that exist. Pass statesChanged when the states are new,
        // which places their nodes again.
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

        bool m_PlaceNodes      = false;
        bool m_ClearSelection  = false;
        int  m_FitViewInFrames = 0;

        PendingAction m_Pending = PendingAction::None;
        std::string   m_PendingPath;
        std::string   m_LoadPath; // read on the next Draw, which has the file system
    };
}
