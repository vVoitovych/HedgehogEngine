#pragma once

#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HedgehogEngine
{
    class ParticleEffectContainer;
}

namespace Editor
{
    // The Particle Effect window (Window > Particle Effect, or opening a .particle in the Project
    // panel): a form over a copy of one effect. The emitter list on the left adds, duplicates,
    // deletes and moves emitters (through ParticleEditing, so sub-emitters keep pointing at theirs);
    // the selected emitter's every field is on the right, its curves and gradient in their editors.
    // Every edit is checked with ValidateParticleEffect: an emitter at fault is listed red and every
    // problem is listed under the form.
    //
    // Files as the animator controller window: New starts an untitled effect of one default emitter,
    // Open... picks a .particle under a mount, Save (Ctrl+S) writes WriteParticleEffect's text (asking
    // where for an untitled one), Save As... writes a new file and goes on editing it, and unsaved
    // edits are asked about (Save, Discard, which reads the file again, Cancel) before New, Open or
    // closing. The engine reloads a saved file within a second, so a preview or Play shows it.
    class ParticleEditorWindow
    {
    public:
        bool Open = false;

        // effects shows a valid edit before it is saved (ParticleEffectContainer::SetOverride), so a
        // preview or Play follows the form; saving, discarding or opening another effect clears it.
        void Draw(const FS::FileSystemManager& fileSystem, HedgehogEngine::ParticleEffectContainer& effects);

        // Shows the window on the effect at virtualPath, asking first when there are unsaved edits.
        void OpenEffect(const std::string& virtualPath);

    private:
        enum class PendingAction
        {
            None,
            OpenEffect, // switch to m_PendingPath
            NewEffect,
            Close,
        };

        void DrawToolbar(const FS::FileSystemManager& fileSystem);
        void DrawUnsavedChangesPopup(const FS::FileSystemManager& fileSystem);
        void DrawEmitterList();
        // The selected emitter's fields; true when one changed.
        bool DrawEmitter(HedgehogEngine::ParticleEmitter& emitter);
        bool DrawSubEmitters(uint32_t index);
        void DrawProblems();

        void RequestOpen(const std::string& virtualPath);
        void RequestNew();
        void Load(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
        void NewEffect();
        void OpenFile(const FS::FileSystemManager& fileSystem);
        void Save(const FS::FileSystemManager& fileSystem);
        void SaveAs(const FS::FileSystemManager& fileSystem);
        bool Write(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);
        // After an edit: the text the dirty check compares, and the problems.
        void ApplyEdit();
        // Drops the edit shown in place of a file, if any.
        void ClearPreview();
        [[nodiscard]] bool HasProblems(const HedgehogEngine::ParticleEmitter& emitter) const;
        [[nodiscard]] bool IsUntitled() const { return m_HasEffect && m_Path.empty(); }
        [[nodiscard]] bool IsDirty() const { return m_HasEffect && m_EditedText != m_SavedText; }

        std::string                    m_Path; // virtual; empty for an untitled effect
        std::filesystem::path          m_File; // physical
        bool                           m_HasEffect = false;
        HedgehogEngine::ParticleEffect m_Edited;
        std::string                    m_EditedText;
        std::string                    m_SavedText;
        std::string                    m_Status;
        std::vector<std::string>       m_Problems;
        uint32_t                       m_Selected = 0;
        std::string                    m_NameBuffer; // the selected emitter's name as typed

        HedgehogEngine::ParticleEffectContainer* m_Effects = nullptr;
        std::string                              m_PreviewText; // the text the container shows, or the file's
        std::string                              m_PreviewPath; // the file shown overridden, empty for none

        PendingAction m_Pending = PendingAction::None;
        std::string   m_PendingPath;
        std::string   m_LoadPath; // read on the next Draw, which has the file system
    };
}
