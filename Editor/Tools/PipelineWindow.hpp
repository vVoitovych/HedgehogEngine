#pragma once

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Editor
{

class PipelineWindow
{
public:
    bool Open = false;

    void Draw(const FS::FileSystemManager& fileSystem);

private:
    struct BindingState
    {
        int  Binding  = 0;
        int  Type     = 0;    // index into descriptor type array
        int  Count    = 1;
        bool Vertex   = false;
        bool Fragment = false;
        bool Compute  = false;
    };

    struct SetState
    {
        std::vector<BindingState> Bindings;
    };

    struct PushConstantState
    {
        int  Offset   = 0;
        int  Size     = 64;
        bool Vertex   = true;
        bool Fragment = false;
        bool Compute  = false;
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
