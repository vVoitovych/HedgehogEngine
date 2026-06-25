#include "ShaderWindow.hpp"

#include "DialogueWindows/api/ShaderDialogue.hpp"
#include "DialogueWindows/api/PipelineDialogue.hpp"
#include "DialogueWindows/api/VertexDescDialogue.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <set>

namespace
{
    // Convert an absolute physical path to an engine:// virtual path by stripping the repo root.
    std::string ToEngineVirtualPath(const std::string& absPath)
    {
        const std::string root = FS::GetEngineRootDirectory().string();
        if (absPath.size() > root.size() && absPath.substr(0, root.size()) == root)
        {
            std::string rel = absPath.substr(root.size());
            // Normalize path separators to forward slashes.
            for (char& c : rel) { if (c == '\\') c = '/'; }
            if (!rel.empty() && rel.front() == '/') rel = rel.substr(1);
            return "engine://" + rel;
        }
        return {};
    }
}

namespace Editor
{

// ── Enum lookup tables ────────────────────────────────────────────────────────
// Order matches ShaderLoader parse functions exactly — index IS the enum value.

namespace
{
    constexpr const char* k_TopologyNames[]    = { "triangle_list", "triangle_strip", "line_list", "point_list" };
    constexpr const char* k_TopologyDisplay[]  = { "Triangle List", "Triangle Strip", "Line List",  "Point List" };
    constexpr int k_TopologyCount = 4;

    constexpr const char* k_CullModeNames[]    = { "none",  "front",  "back"       };
    constexpr const char* k_CullModeDisplay[]  = { "None",  "Front",  "Back"       };
    constexpr int k_CullModeCount = 3;

    constexpr const char* k_FillModeNames[]    = { "solid",  "wireframe"    };
    constexpr const char* k_FillModeDisplay[]  = { "Solid",  "Wireframe"    };
    constexpr int k_FillModeCount = 2;

    constexpr const char* k_CompareOpNames[]   = { "never","less","equal","less_or_equal","greater","not_equal","greater_or_equal","always" };
    constexpr const char* k_CompareOpDisplay[] = { "Never","Less","Equal","Less Or Equal","Greater","Not Equal","Greater Or Equal","Always" };
    constexpr int k_CompareOpCount = 8;

    constexpr const char* k_BlendFactorNames[]   = { "zero","one","src_alpha","one_minus_src_alpha","dst_alpha","one_minus_dst_alpha","src_color","one_minus_src_color" };
    constexpr const char* k_BlendFactorDisplay[] = { "Zero","One","Src Alpha","1-Src Alpha","Dst Alpha","1-Dst Alpha","Src Color","1-Src Color" };
    constexpr int k_BlendFactorCount = 8;

    constexpr const char* k_BlendOpNames[]   = { "add","subtract","reverse_subtract","min","max" };
    constexpr const char* k_BlendOpDisplay[] = { "Add","Subtract","Rev Subtract","Min","Max" };
    constexpr int k_BlendOpCount = 5;

    constexpr const char* k_StageYaml[]    = { "vertex", "fragment", "compute" };
    constexpr const char* k_StageDisplay[] = { "Vertex", "Fragment", "Compute" };
    constexpr int k_StageCount = 3;

    template<int N>
    int FindIndex(const char* const (&names)[N], const std::string& s, int fallback = 0)
    {
        for (int i = 0; i < N; ++i)
            if (s == names[i]) return i;
        return fallback;
    }
}

// ── Enum helpers ──────────────────────────────────────────────────────────────

int  ShaderWindow::TopologyToIndex(const std::string& s)  { return FindIndex(k_TopologyNames,    s, 0); }
int  ShaderWindow::CullModeToIndex(const std::string& s)  { return FindIndex(k_CullModeNames,    s, 2); }
int  ShaderWindow::FillModeToIndex(const std::string& s)  { return FindIndex(k_FillModeNames,    s, 0); }
int  ShaderWindow::CompareOpToIndex(const std::string& s) { return FindIndex(k_CompareOpNames,   s, 1); }
int  ShaderWindow::BlendFactorToIndex(const std::string& s){ return FindIndex(k_BlendFactorNames, s, 1); }
int  ShaderWindow::BlendOpToIndex(const std::string& s)   { return FindIndex(k_BlendOpNames,     s, 0); }
int  ShaderWindow::StageToIndex(const std::string& s)     { return FindIndex(k_StageYaml,        s, 0); }

const char* ShaderWindow::IndexToTopology(int i)     { return (i >= 0 && i < k_TopologyCount)    ? k_TopologyNames[i]    : k_TopologyNames[0];    }
const char* ShaderWindow::IndexToCullMode(int i)     { return (i >= 0 && i < k_CullModeCount)    ? k_CullModeNames[i]    : k_CullModeNames[2];    }
const char* ShaderWindow::IndexToFillMode(int i)     { return (i >= 0 && i < k_FillModeCount)    ? k_FillModeNames[i]    : k_FillModeNames[0];    }
const char* ShaderWindow::IndexToCompareOp(int i)    { return (i >= 0 && i < k_CompareOpCount)   ? k_CompareOpNames[i]   : k_CompareOpNames[1];   }
const char* ShaderWindow::IndexToBlendFactor(int i)  { return (i >= 0 && i < k_BlendFactorCount) ? k_BlendFactorNames[i] : k_BlendFactorNames[1]; }
const char* ShaderWindow::IndexToBlendOp(int i)      { return (i >= 0 && i < k_BlendOpCount)     ? k_BlendOpNames[i]     : k_BlendOpNames[0];     }
const char* ShaderWindow::IndexToStage(int i)        { return (i >= 0 && i < k_StageCount)       ? k_StageYaml[i]        : k_StageYaml[0];        }

// ── Helpers ───────────────────────────────────────────────────────────────────

bool ShaderWindow::InputPath(const char* label, std::string& path, float width)
{
    char buf[512] = {};
    const size_t n = std::min(path.size(), sizeof(buf) - 1);
    std::memcpy(buf, path.c_str(), n);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText(label, buf, sizeof(buf)))
    {
        path = buf;
        return true;
    }
    return false;
}

std::string ShaderWindow::MakeRelativePath(const std::string& absPath) const
{
    if (m_FilePath.empty())
        return absPath;
    const auto shaderDir = std::filesystem::path(m_FilePath).parent_path();
    const auto rel       = std::filesystem::path(absPath).lexically_relative(shaderDir);
    return rel.generic_string();
}

bool ShaderWindow::HasDuplicateStage(const std::string& stage, int skipRow) const
{
    for (int i = 0; i < static_cast<int>(m_Stages.size()); ++i)
        if (i != skipRow && m_Stages[i].m_Stage == stage)
            return true;
    return false;
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void ShaderWindow::NewFile()
{
    m_FilePath.clear();
    m_VirtualPath.clear();
    m_PipelineLayout.clear();
    m_VertexDescription.clear();
    m_Stages.clear();
    m_BlendAttachments.clear();

    m_PipelineType  = PipelineType::Graphics;
    m_Topology      = 0;
    m_CullMode      = 2;
    m_FillMode      = 0;
    m_DepthTest     = true;
    m_DepthWrite    = true;
    m_DepthCompare  = 1;

    m_Stages.push_back({ "vertex", "" });
    m_Dirty = false;
}

void ShaderWindow::OpenFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::ShaderOpenDialogue();
    if (path != nullptr)
        LoadFromPath(path, fileSystem);
}

void ShaderWindow::SaveFile(const FS::FileSystemManager& fileSystem)
{
    if (m_VirtualPath.empty())
        SaveAsFile(fileSystem);
    else
        SaveToPath(m_VirtualPath, fileSystem);
}

void ShaderWindow::SaveAsFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::ShaderSaveDialogue();
    if (path != nullptr)
    {
        m_FilePath    = path;
        m_VirtualPath = ToEngineVirtualPath(path);
        SaveToPath(m_VirtualPath, fileSystem);
    }
}

bool ShaderWindow::LoadFromPath(const std::string& path, const FS::FileSystemManager& fileSystem)
{
    const std::string virtualPath = ToEngineVirtualPath(path);
    if (virtualPath.empty())
    {
        LOGERROR("ShaderWindow: '", path, "' is outside the engine root and cannot be opened.");
        return false;
    }

    const auto text = fileSystem.ReadTextFile(virtualPath);
    if (!text)
        return false;

    YAML::Node root;
    try { root = YAML::Load(*text); }
    catch (const YAML::Exception&) { return false; }

    m_PipelineLayout.clear();
    m_VertexDescription.clear();
    m_Stages.clear();
    m_BlendAttachments.clear();

    // Required references
    if (const YAML::Node& n = root["pipeline_layout"])
        m_PipelineLayout = n.as<std::string>();
    if (const YAML::Node& n = root["vertex_description"])
        m_VertexDescription = n.as<std::string>();

    // Topology
    m_Topology = 0;
    if (const YAML::Node& n = root["topology"])
        m_Topology = TopologyToIndex(n.as<std::string>());

    // Rasterization
    m_CullMode = 2;
    m_FillMode = 0;
    if (const YAML::Node& n = root["rasterization"])
    {
        if (const YAML::Node& cm = n["cull_mode"]) m_CullMode = CullModeToIndex(cm.as<std::string>());
        if (const YAML::Node& fm = n["fill_mode"])  m_FillMode = FillModeToIndex(fm.as<std::string>());
    }

    // Depth
    m_DepthTest = true; m_DepthWrite = true; m_DepthCompare = 1;
    if (const YAML::Node& n = root["depth"])
    {
        if (const YAML::Node& t = n["test"])    m_DepthTest    = t.as<bool>();
        if (const YAML::Node& w = n["write"])   m_DepthWrite   = w.as<bool>();
        if (const YAML::Node& c = n["compare"]) m_DepthCompare = CompareOpToIndex(c.as<std::string>());
    }

    // Blend attachments
    if (const YAML::Node& blends = root["blend"])
    {
        for (const YAML::Node& b : blends)
        {
            BlendAttachment att;
            if (const YAML::Node& e = b["enabled"])   att.m_Enabled  = e.as<bool>();
            if (const YAML::Node& f = b["src_color"])  att.m_SrcColor = BlendFactorToIndex(f.as<std::string>());
            if (const YAML::Node& f = b["dst_color"])  att.m_DstColor = BlendFactorToIndex(f.as<std::string>());
            if (const YAML::Node& o = b["color_op"])   att.m_ColorOp  = BlendOpToIndex(o.as<std::string>());
            if (const YAML::Node& f = b["src_alpha"])  att.m_SrcAlpha = BlendFactorToIndex(f.as<std::string>());
            if (const YAML::Node& f = b["dst_alpha"])  att.m_DstAlpha = BlendFactorToIndex(f.as<std::string>());
            if (const YAML::Node& o = b["alpha_op"])   att.m_AlphaOp  = BlendOpToIndex(o.as<std::string>());
            m_BlendAttachments.push_back(att);
        }
    }

    // Shader stages — also infer pipeline type
    bool hasCompute = false;
    bool hasGraphics = false;
    if (const YAML::Node& stages = root["shaders"])
    {
        for (const YAML::Node& s : stages)
        {
            StageEntry e;
            e.m_Stage = s["stage"] ? s["stage"].as<std::string>() : "vertex";
            e.m_Path  = s["path"]  ? s["path"].as<std::string>()  : "";
            if (e.m_Stage == "compute")                     hasCompute  = true;
            if (e.m_Stage == "vertex" || e.m_Stage == "fragment") hasGraphics = true;
            m_Stages.push_back(std::move(e));
        }
    }
    m_PipelineType = (hasCompute && !hasGraphics) ? PipelineType::Compute : PipelineType::Graphics;

    m_FilePath    = path;
    m_VirtualPath = virtualPath;
    m_Dirty       = false;
    return true;
}

bool ShaderWindow::SaveToPath(const std::string& virtualPath,
                               const FS::FileSystemManager& fileSystem)
{
    if (virtualPath.empty())
    {
        LOGERROR("ShaderWindow: cannot save — file path is outside the engine root.");
        return false;
    }

    const bool isGraphics = (m_PipelineType == PipelineType::Graphics);

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "pipeline_layout" << YAML::Value << m_PipelineLayout;

    if (isGraphics && !m_VertexDescription.empty())
        out << YAML::Key << "vertex_description" << YAML::Value << m_VertexDescription;

    if (isGraphics)
    {
        out << YAML::Key << "topology" << YAML::Value << IndexToTopology(m_Topology);

        out << YAML::Key << "rasterization" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "cull_mode" << YAML::Value << IndexToCullMode(m_CullMode);
        out << YAML::Key << "fill_mode" << YAML::Value << IndexToFillMode(m_FillMode);
        out << YAML::EndMap;

        out << YAML::Key << "depth" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "test"    << YAML::Value << m_DepthTest;
        out << YAML::Key << "write"   << YAML::Value << m_DepthWrite;
        out << YAML::Key << "compare" << YAML::Value << IndexToCompareOp(m_DepthCompare);
        out << YAML::EndMap;

        if (!m_BlendAttachments.empty())
        {
            out << YAML::Key << "blend" << YAML::Value << YAML::BeginSeq;
            for (const auto& att : m_BlendAttachments)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "enabled"    << YAML::Value << att.m_Enabled;
                out << YAML::Key << "src_color"   << YAML::Value << IndexToBlendFactor(att.m_SrcColor);
                out << YAML::Key << "dst_color"   << YAML::Value << IndexToBlendFactor(att.m_DstColor);
                out << YAML::Key << "color_op"    << YAML::Value << IndexToBlendOp(att.m_ColorOp);
                out << YAML::Key << "src_alpha"   << YAML::Value << IndexToBlendFactor(att.m_SrcAlpha);
                out << YAML::Key << "dst_alpha"   << YAML::Value << IndexToBlendFactor(att.m_DstAlpha);
                out << YAML::Key << "alpha_op"    << YAML::Value << IndexToBlendOp(att.m_AlphaOp);
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
        }
    }

    out << YAML::Key << "shaders" << YAML::Value << YAML::BeginSeq;
    for (const auto& s : m_Stages)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "stage" << YAML::Value << s.m_Stage;
        out << YAML::Key << "path"  << YAML::Value << s.m_Path;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;

    if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
        return false;

    m_Dirty = false;
    return true;
}

// ── UI sections ───────────────────────────────────────────────────────────────

void ShaderWindow::DrawFileControls(const FS::FileSystemManager& fileSystem)
{
    if (ImGui::Button("New"))     NewFile();
    ImGui::SameLine();
    if (ImGui::Button("Open"))    OpenFile(fileSystem);
    ImGui::SameLine();
    if (ImGui::Button("Save"))    SaveFile(fileSystem);
    ImGui::SameLine();
    if (ImGui::Button("Save As")) SaveAsFile(fileSystem);

    ImGui::Separator();

    const std::string display = m_VirtualPath.empty()
        ? (m_FilePath.empty() ? "(unsaved)" : m_FilePath)
        : m_VirtualPath;
    ImGui::TextDisabled("File: %s%s", display.c_str(), m_Dirty ? "  *" : "");
}

void ShaderWindow::DrawPipelineType()
{
    if (!ImGui::CollapsingHeader("Pipeline Type", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    int type = static_cast<int>(m_PipelineType);
    if (ImGui::RadioButton("Graphics", &type, 0)) { m_PipelineType = PipelineType::Graphics; m_Dirty = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Compute",  &type, 1)) { m_PipelineType = PipelineType::Compute;  m_Dirty = true; }

    if (m_PipelineType == PipelineType::Compute)
        ImGui::TextDisabled("Compute: pipeline state and vertex description are not applicable.");
}

void ShaderWindow::DrawReferences()
{
    if (!ImGui::CollapsingHeader("References", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // Pipeline layout (.pl) — required for all pipeline types
    const bool missingPl = m_PipelineLayout.empty();
    ImGui::Text("Pipeline Layout (.pl):");
    if (missingPl) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
    if (InputPath("##pl", m_PipelineLayout, -85.0f)) m_Dirty = true;
    if (missingPl) ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::Button("Browse##pl"))
    {
        char* p = DialogueWindows::PipelineOpenDialogue();
        if (p) { m_PipelineLayout = MakeRelativePath(p); m_Dirty = true; }
    }

    // Vertex description (.vdes) — graphics only
    if (m_PipelineType == PipelineType::Graphics)
    {
        ImGui::Text("Vertex Description (.vdes):");
        if (InputPath("##vd", m_VertexDescription, -85.0f)) m_Dirty = true;
        ImGui::SameLine();
        if (ImGui::Button("Browse##vd"))
        {
            char* p = DialogueWindows::VertexDescOpenDialogue();
            if (p) { m_VertexDescription = MakeRelativePath(p); m_Dirty = true; }
        }
    }
}

void ShaderWindow::DrawShaderStages()
{
    if (!ImGui::CollapsingHeader("Shader Stages", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##stage_tbl", 5, flags))
    {
        ImGui::TableSetupColumn("#",          ImGuiTableColumnFlags_WidthFixed,   28.0f);
        ImGui::TableSetupColumn("Stage",      ImGuiTableColumnFlags_WidthFixed,   88.0f);
        ImGui::TableSetupColumn("SPIR-V Path",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##browse",   ImGuiTableColumnFlags_WidthFixed,   60.0f);
        ImGui::TableSetupColumn("##del",      ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableHeadersRow();

        int toDelete = -1;
        for (int i = 0; i < static_cast<int>(m_Stages.size()); ++i)
        {
            auto& s = m_Stages[i];
            const bool dupStage  = HasDuplicateStage(s.m_Stage, i);
            const bool emptyPath = s.m_Path.empty();

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // Row index
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            // Stage combo — red when duplicate
            ImGui::TableSetColumnIndex(1);
            if (dupStage) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            int stageIdx = StageToIndex(s.m_Stage);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##stg", &stageIdx, k_StageDisplay, k_StageCount))
            {
                s.m_Stage = IndexToStage(stageIdx);
                m_Dirty   = true;
            }
            if (dupStage) ImGui::PopStyleColor();

            // SPIR-V path — orange when empty
            ImGui::TableSetColumnIndex(2);
            if (emptyPath) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));
            if (InputPath("##spv", s.m_Path, -FLT_MIN)) m_Dirty = true;
            if (emptyPath) ImGui::PopStyleColor();

            // Browse for .spv
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("Browse"))
            {
                char* p = DialogueWindows::SpvOpenDialogue();
                if (p) { s.m_Path = MakeRelativePath(p); m_Dirty = true; }
            }

            // Delete
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("X")) toDelete = i;

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (toDelete >= 0)
        {
            m_Stages.erase(m_Stages.begin() + toDelete);
            m_Dirty = true;
        }
    }

    if (ImGui::Button("+ Add stage"))
    {
        const std::string defaultStage =
            (m_PipelineType == PipelineType::Compute) ? "compute" : "vertex";
        m_Stages.push_back({ defaultStage, "" });
        m_Dirty = true;
    }
}

void ShaderWindow::DrawPipelineState()
{
    if (m_PipelineType != PipelineType::Graphics)
        return;

    if (!ImGui::CollapsingHeader("Pipeline State", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // ── Topology ──────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Input Assembly");
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::Combo("Topology##topo", &m_Topology, k_TopologyDisplay, k_TopologyCount))
        m_Dirty = true;

    // ── Rasterization ─────────────────────────────────────────────────────────
    ImGui::SeparatorText("Rasterization");
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("Cull Mode##cull", &m_CullMode, k_CullModeDisplay, k_CullModeCount))
        m_Dirty = true;
    ImGui::SameLine(0.0f, 20.0f);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::Combo("Fill Mode##fill", &m_FillMode, k_FillModeDisplay, k_FillModeCount))
        m_Dirty = true;

    // ── Depth ─────────────────────────────────────────────────────────────────
    ImGui::SeparatorText("Depth");
    if (ImGui::Checkbox("Test##dt",  &m_DepthTest))  m_Dirty = true;
    ImGui::SameLine(0.0f, 16.0f);
    if (ImGui::Checkbox("Write##dw", &m_DepthWrite)) m_Dirty = true;
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::Combo("Compare##cmp", &m_DepthCompare, k_CompareOpDisplay, k_CompareOpCount))
        m_Dirty = true;

    // ── Blend attachments ─────────────────────────────────────────────────────
    ImGui::SeparatorText("Blend Attachments");

    int toRemove = -1;
    for (int i = 0; i < static_cast<int>(m_BlendAttachments.size()); ++i)
    {
        auto& att = m_BlendAttachments[i];
        ImGui::PushID(i);

        const bool open = ImGui::TreeNodeEx(
            "##att",
            ImGuiTreeNodeFlags_DefaultOpen,
            "Attachment %d  (%s)",
            i, att.m_Enabled ? "blend on" : "blend off");

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::SmallButton("Remove")) toRemove = i;
        ImGui::PopStyleColor(2);

        if (open)
        {
            if (ImGui::Checkbox("Blend Enabled##en", &att.m_Enabled)) m_Dirty = true;

            if (att.m_Enabled)
            {
                // Color row
                ImGui::Text("Color:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::Combo("Src##sc",  &att.m_SrcColor, k_BlendFactorDisplay, k_BlendFactorCount)) m_Dirty = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo("Op##co",   &att.m_ColorOp,  k_BlendOpDisplay,     k_BlendOpCount))     m_Dirty = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::Combo("Dst##dc",  &att.m_DstColor, k_BlendFactorDisplay, k_BlendFactorCount)) m_Dirty = true;

                // Alpha row
                ImGui::Text("Alpha:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::Combo("Src##sa",  &att.m_SrcAlpha, k_BlendFactorDisplay, k_BlendFactorCount)) m_Dirty = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Combo("Op##ao",   &att.m_AlphaOp,  k_BlendOpDisplay,     k_BlendOpCount))     m_Dirty = true;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::Combo("Dst##da",  &att.m_DstAlpha, k_BlendFactorDisplay, k_BlendFactorCount)) m_Dirty = true;
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (toRemove >= 0)
    {
        m_BlendAttachments.erase(m_BlendAttachments.begin() + toRemove);
        m_Dirty = true;
    }

    if (ImGui::Button("+ Add blend attachment"))
    {
        m_BlendAttachments.push_back(BlendAttachment{});
        m_Dirty = true;
    }
}

void ShaderWindow::DrawValidation()
{
    const bool noData = m_Stages.empty() && m_PipelineLayout.empty();
    if (noData)
    {
        ImGui::TextDisabled("No data. Fill in references and add shader stages above.");
        return;
    }

    bool anyIssue = false;

    auto error = [&](const char* fmt, ...)
    {
        char buf[256];
        va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("%s", buf);
        ImGui::PopStyleColor();
        anyIssue = true;
    };

    auto warn = [&](const char* fmt, ...)
    {
        char buf[256];
        va_list args; va_start(args, fmt); vsnprintf(buf, sizeof(buf), fmt, args); va_end(args);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
        ImGui::Text("%s", buf);
        ImGui::PopStyleColor();
        anyIssue = true;
    };

    // Pipeline layout required
    if (m_PipelineLayout.empty())
        error("[error] Pipeline layout (.pl) path is required");

    if (m_PipelineType == PipelineType::Graphics)
    {
        // Vertex stage required
        const bool hasVertex = std::any_of(m_Stages.begin(), m_Stages.end(),
            [](const StageEntry& e){ return e.m_Stage == "vertex"; });
        if (!hasVertex)
            error("[error] Graphics pipeline requires a vertex shader stage");

        // Compute stage in graphics
        for (const auto& s : m_Stages)
            if (s.m_Stage == "compute")
                warn("[warn]  Compute stage in a graphics pipeline");
    }
    else // Compute
    {
        const bool hasCompute = std::any_of(m_Stages.begin(), m_Stages.end(),
            [](const StageEntry& e){ return e.m_Stage == "compute"; });
        if (!hasCompute)
            error("[error] Compute pipeline requires a compute shader stage");

        for (const auto& s : m_Stages)
            if (s.m_Stage == "vertex" || s.m_Stage == "fragment")
                warn("[warn]  Graphics stage (%s) in a compute pipeline", s.m_Stage.c_str());
    }

    // Duplicate stages
    std::set<std::string> seenStages;
    for (const auto& s : m_Stages)
    {
        if (!seenStages.insert(s.m_Stage).second)
            error("[error] Duplicate shader stage: %s", s.m_Stage.c_str());
    }

    // Empty SPIR-V paths
    for (int i = 0; i < static_cast<int>(m_Stages.size()); ++i)
        if (m_Stages[i].m_Path.empty())
            error("[error] Stage %d (%s): SPIR-V path is empty", i, m_Stages[i].m_Stage.c_str());

    if (!anyIssue)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 0.4f, 1.0f));
        ImGui::Text("OK");
        ImGui::PopStyleColor();
    }
}

void ShaderWindow::Draw(const FS::FileSystemManager& fileSystem)
{
    if (!m_Open)
        return;

    const std::string title =
        std::string("Shader") + (m_Dirty ? " *" : "") + "###Shader";

    ImGui::SetNextWindowSize(ImVec2(680.0f, 620.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::End();
        return;
    }

    DrawFileControls(fileSystem);
    ImGui::Spacing();
    DrawPipelineType();
    ImGui::Spacing();
    DrawReferences();
    ImGui::Spacing();
    DrawShaderStages();
    ImGui::Spacing();
    DrawPipelineState();
    ImGui::Spacing();
    ImGui::SeparatorText("Validation");
    DrawValidation();

    ImGui::End();
}

} // namespace Editor
