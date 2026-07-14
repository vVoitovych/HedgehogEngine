#pragma once

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>
#include <vector>

namespace Editor
{

class VertexDescriptionWindow
{
public:
    bool m_Open = false;

    void Draw(const FS::FileSystemManager& fileSystem);

private:
    struct BindingState
    {
        int m_Binding   = 0;
        int m_Stride    = 12;
        int m_InputRate = 0;    // 0 = per_vertex, 1 = per_instance
    };

    struct AttributeState
    {
        int m_Location = 0;
        int m_Binding  = 0;
        int m_Format   = 2;    // default: r32g32b32_float
        int m_Offset   = 0;
    };

    // ── Sub-sections ──────────────────────────────────────────────────────────
    void DrawFileControls(const FS::FileSystemManager& fileSystem);
    void DrawBindingsTable();
    void DrawAttributesTable();
    void DrawValidation();

    // ── Validation helpers ────────────────────────────────────────────────────
    bool HasDuplicateBindingIndex(int index, int skipRow) const;
    bool HasDuplicateAttributeLocation(int location, int skipRow) const;
    bool AttributeBindingExists(int binding) const;

    // ── File I/O ──────────────────────────────────────────────────────────────
    void NewFile();
    void OpenFile(const FS::FileSystemManager& fileSystem);
    void SaveFile(const FS::FileSystemManager& fileSystem);
    void SaveAsFile(const FS::FileSystemManager& fileSystem);
    bool LoadFromPath(const std::string& path, const FS::FileSystemManager& fileSystem);
    bool SaveToPath(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);

    // ── Format / input-rate string mapping ───────────────────────────────────
    static int         FormatToIndex(const std::string& fmt);
    static const char* IndexToFormat(int idx);
    static const char* IndexToInputRate(int idx);

    // ── State ─────────────────────────────────────────────────────────────────
    std::string                 m_FilePath;     // absolute OS path — used for display
    std::string                 m_VirtualPath;  // engine:// path — used for writing
    bool                        m_Dirty = false;
    std::vector<BindingState>   m_Bindings;
    std::vector<AttributeState> m_Attributes;
};

} // namespace Editor
