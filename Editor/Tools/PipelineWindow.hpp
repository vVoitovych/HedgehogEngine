#pragma once

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Editor
{

class PipelineWindow
{
public:
    bool m_Open = false;

    void Draw(const FS::FileSystemManager& fileSystem);

private:
    struct BindingState
    {
        int  m_Binding  = 0;
        int  m_Type     = 0;    // index into descriptor type array
        int  m_Count    = 1;
        bool m_Vertex   = false;
        bool m_Fragment = false;
        bool m_Compute  = false;
    };

    struct SetState
    {
        std::vector<BindingState> m_Bindings;
    };

    struct PushConstantState
    {
        int  m_Offset   = 0;
        int  m_Size     = 64;
        bool m_Vertex   = true;
        bool m_Fragment = false;
        bool m_Compute  = false;
    };

    // ── Sub-sections ──────────────────────────────────────────────────────────
    void DrawFileControls(const FS::FileSystemManager& fileSystem);
    void DrawDescriptorSets();
    void DrawPushConstants();
    void DrawValidation();

    // ── Validation helpers ────────────────────────────────────────────────────
    bool HasDuplicateBinding(int setIdx, int binding, int skipRow) const;
    bool NoStagesSelected(bool v, bool f, bool c) const { return !v && !f && !c; }
    bool PushConstantsOverlap(int i, int j) const;

    // ── File I/O ──────────────────────────────────────────────────────────────
    void NewFile();
    void OpenFile(const FS::FileSystemManager& fileSystem);
    void SaveFile(const FS::FileSystemManager& fileSystem);
    void SaveAsFile(const FS::FileSystemManager& fileSystem);
    bool LoadFromPath(const std::string& path, const FS::FileSystemManager& fileSystem);
    bool SaveToPath(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);

    // ── Stage string helpers ──────────────────────────────────────────────────
    static void        ParseStageString(const std::string& s, bool& v, bool& f, bool& c);
    static std::string StagesToString(bool v, bool f, bool c);

    // ── Descriptor type helpers ───────────────────────────────────────────────
    static int         TypeNameToIndex(const std::string& name);
    static const char* IndexToTypeName(int idx);
    static const char* IndexToTypeDisplay(int idx);

    // ── State ─────────────────────────────────────────────────────────────────
    std::string                    m_FilePath;     // absolute OS path — used for writing
    std::string                    m_VirtualPath;  // engine:// path — used for display
    bool                           m_Dirty = false;
    std::vector<SetState>          m_Sets;
    std::vector<PushConstantState> m_PushConstants;
};

} // namespace Editor
