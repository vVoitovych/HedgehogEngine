#pragma once

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Editor
{

class ShaderWindow
{
public:
    bool Open = false;

    void Draw(const FS::FileSystemManager& fileSystem);

private:
    enum class PipelineType { Graphics = 0, Compute = 1 };

    struct StageEntry
    {
        std::string Stage;   // "vertex" | "fragment" | "compute"
        std::string Path;    // relative path to .spv
    };

    struct BlendAttachment
    {
        bool Enabled   = false;
        int  SrcColor  = 1;   // "one"
        int  DstColor  = 0;   // "zero"
        int  ColorOp   = 0;   // "add"
        int  SrcAlpha  = 1;   // "one"
        int  DstAlpha  = 0;   // "zero"
        int  AlphaOp   = 0;   // "add"
    };

    // ── Sub-sections ──────────────────────────────────────────────────────────
    void DrawFileControls(const FS::FileSystemManager& fileSystem);
    void DrawPipelineType();
    void DrawReferences();
    void DrawShaderStages();
    void DrawPipelineState();
    void DrawValidation();

    // ── File I/O ──────────────────────────────────────────────────────────────
    void NewFile();
    void OpenFile(const FS::FileSystemManager& fileSystem);
    void SaveFile(const FS::FileSystemManager& fileSystem);
    void SaveAsFile(const FS::FileSystemManager& fileSystem);
    bool LoadFromPath(const std::string& path, const FS::FileSystemManager& fileSystem);
    bool SaveToPath(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);

    // ── Helpers ───────────────────────────────────────────────────────────────
    std::string MakeRelativePath(const std::string& absPath) const;
    static bool InputPath(const char* label, std::string& path, float width = -FLT_MIN);
    bool HasDuplicateStage(const std::string& stage, int skipRow) const;

    // ── Enum string tables (names = YAML tokens, indices are array positions) ─
    static int         TopologyToIndex(const std::string& s);
    static const char* IndexToTopology(int i);
    static int         CullModeToIndex(const std::string& s);
    static const char* IndexToCullMode(int i);
    static int         FillModeToIndex(const std::string& s);
    static const char* IndexToFillMode(int i);
    static int         CompareOpToIndex(const std::string& s);
    static const char* IndexToCompareOp(int i);
    static int         BlendFactorToIndex(const std::string& s);
    static const char* IndexToBlendFactor(int i);
    static int         BlendOpToIndex(const std::string& s);
    static const char* IndexToBlendOp(int i);
    static int         StageToIndex(const std::string& s);
    static const char* IndexToStage(int i);

    // ── State ─────────────────────────────────────────────────────────────────
    std::string  m_FilePath;     // absolute OS path — used for writing and relative-path computation
    std::string  m_VirtualPath;  // engine:// path — used for display
    bool         m_Dirty = false;

    PipelineType m_PipelineType = PipelineType::Graphics;

    std::string  m_PipelineLayout;       // relative path to .pl
    std::string  m_VertexDescription;    // relative path to .vdes

    // Graphics pipeline state
    int  m_Topology     = 0;    // triangle_list
    int  m_CullMode     = 2;    // back
    int  m_FillMode     = 0;    // solid
    bool m_DepthTest    = true;
    bool m_DepthWrite   = true;
    int  m_DepthCompare = 1;    // less

    std::vector<BlendAttachment> m_BlendAttachments;
    std::vector<StageEntry>      m_Stages;
};

} // namespace Editor
