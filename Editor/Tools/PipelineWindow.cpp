#include "PipelineWindow.hpp"

#include "DialogueWindows/api/PipelineDialogue.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <set>
#include <sstream>

namespace Editor
{

namespace
{
    // YAML token names — must match PipelineLoader exactly
    constexpr const char* k_TypeNames[] = {
        "uniform_buffer",
        "storage_buffer",
        "combined_image_sampler",
        "storage_image",
        "input_attachment",
    };
    constexpr const char* k_TypeDisplayNames[] = {
        "Uniform Buffer",
        "Storage Buffer",
        "Combined Image Sampler",
        "Storage Image",
        "Input Attachment",
    };
    constexpr int k_TypeCount = 5;
}

// ── Descriptor type helpers ───────────────────────────────────────────────────

int PipelineWindow::TypeNameToIndex(const std::string& name)
{
    for (int i = 0; i < k_TypeCount; ++i)
        if (name == k_TypeNames[i])
            return i;
    return 0;
}

const char* PipelineWindow::IndexToTypeName(int idx)
{
    return (idx >= 0 && idx < k_TypeCount) ? k_TypeNames[idx] : k_TypeNames[0];
}

const char* PipelineWindow::IndexToTypeDisplay(int idx)
{
    return (idx >= 0 && idx < k_TypeCount) ? k_TypeDisplayNames[idx] : k_TypeDisplayNames[0];
}

// ── Stage string helpers ──────────────────────────────────────────────────────

void PipelineWindow::ParseStageString(const std::string& s, bool& v, bool& f, bool& c)
{
    v = f = c = false;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '|'))
    {
        while (!token.empty() && token.front() == ' ') token.erase(token.begin());
        while (!token.empty() && token.back()  == ' ') token.pop_back();
        if      (token == "vertex")   v = true;
        else if (token == "fragment") f = true;
        else if (token == "compute")  c = true;
        else if (token == "all")      { v = f = c = true; }
    }
}

std::string PipelineWindow::StagesToString(bool v, bool f, bool c)
{
    std::string result;
    auto append = [&](const char* s)
    {
        if (!result.empty()) result += " | ";
        result += s;
    };
    if (v) append("vertex");
    if (f) append("fragment");
    if (c) append("compute");
    return result;
}

// ── Validation helpers ────────────────────────────────────────────────────────

bool PipelineWindow::HasDuplicateBinding(int setIdx, int binding, int skipRow) const
{
    const auto& bindings = m_Sets[setIdx].Bindings;
    for (int i = 0; i < static_cast<int>(bindings.size()); ++i)
        if (i != skipRow && bindings[i].Binding == binding)
            return true;
    return false;
}

bool PipelineWindow::PushConstantsOverlap(int i, int j) const
{
    const auto& a = m_PushConstants[i];
    const auto& b = m_PushConstants[j];
    const int aEnd = a.Offset + a.Size;
    const int bEnd = b.Offset + b.Size;
    return (a.Offset < bEnd) && (b.Offset < aEnd);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void PipelineWindow::NewFile()
{
    m_FilePath.clear();
    m_VirtualPath.clear();
    m_Sets.clear();
    m_PushConstants.clear();
    m_Dirty = false;
}

void PipelineWindow::OpenFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::PipelineOpenDialogue();
    if (path != nullptr)
        LoadFromPath(path, fileSystem);
}

void PipelineWindow::SaveFile(const FS::FileSystemManager& fileSystem)
{
    if (m_VirtualPath.empty())
        SaveAsFile(fileSystem);
    else
        SaveToPath(m_VirtualPath, fileSystem);
}

void PipelineWindow::SaveAsFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::PipelineSaveDialogue();
    if (path != nullptr)
    {
        const auto virtualPath = fileSystem.ToVirtualPath(path);
        if (!virtualPath)
        {
            LOGERROR("PipelineWindow: '", path, "' is outside all mounted roots and cannot be saved.");
            return;
        }
        m_FilePath    = path;
        m_VirtualPath = *virtualPath;
        SaveToPath(m_VirtualPath, fileSystem);
    }
}

bool PipelineWindow::LoadFromPath(const std::string& path, const FS::FileSystemManager& fileSystem)
{
    const auto virtualPath = fileSystem.ToVirtualPath(path);
    if (!virtualPath)
    {
        LOGERROR("PipelineWindow: '", path, "' is outside all mounted roots and cannot be opened.");
        return false;
    }

    const auto text = fileSystem.ReadTextFile(*virtualPath);
    if (!text)
        return false;

    YAML::Node root;
    try
    {
        root = YAML::Load(*text);
    }
    catch (const YAML::Exception&)
    {
        return false;
    }

    m_Sets.clear();
    m_PushConstants.clear();

    if (const YAML::Node& sets = root["descriptor_sets"])
    {
        for (const YAML::Node& setNode : sets)
        {
            SetState set;
            if (const YAML::Node& bindingsNode = setNode["bindings"])
            {
                for (const YAML::Node& b : bindingsNode)
                {
                    BindingState bs;
                    bs.Binding = b["binding"].as<int>(0);
                    bs.Type    = TypeNameToIndex(b["type"] ? b["type"].as<std::string>() : "uniform_buffer");
                    bs.Count   = b["count"].as<int>(1);
                    ParseStageString(b["stage"] ? b["stage"].as<std::string>() : "vertex",
                                     bs.Vertex, bs.Fragment, bs.Compute);
                    set.Bindings.push_back(bs);
                }
            }
            m_Sets.push_back(std::move(set));
        }
    }

    if (const YAML::Node& pcs = root["push_constants"])
    {
        for (const YAML::Node& pc : pcs)
        {
            PushConstantState ps;
            ps.Offset = pc["offset"].as<int>(0);
            ps.Size   = pc["size"].as<int>(0);
            ParseStageString(pc["stage"] ? pc["stage"].as<std::string>() : "vertex",
                             ps.Vertex, ps.Fragment, ps.Compute);
            m_PushConstants.push_back(ps);
        }
    }

    m_FilePath    = path;
    m_VirtualPath = *virtualPath;
    m_Dirty       = false;
    return true;
}

bool PipelineWindow::SaveToPath(const std::string& virtualPath,
                                 const FS::FileSystemManager& fileSystem)
{
    if (virtualPath.empty())
    {
        LOGERROR("PipelineWindow: cannot save — file path is outside the engine root.");
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "descriptor_sets" << YAML::Value << YAML::BeginSeq;
    for (const auto& set : m_Sets)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
        for (const auto& b : set.Bindings)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "binding" << YAML::Value << b.Binding;
            out << YAML::Key << "type"    << YAML::Value << IndexToTypeName(b.Type);
            out << YAML::Key << "stage"   << YAML::Value << StagesToString(b.Vertex, b.Fragment, b.Compute);
            out << YAML::Key << "count"   << YAML::Value << b.Count;
            out << YAML::EndMap;
        }
        out << YAML::EndSeq;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "push_constants" << YAML::Value << YAML::BeginSeq;
    for (const auto& pc : m_PushConstants)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "stage"  << YAML::Value << StagesToString(pc.Vertex, pc.Fragment, pc.Compute);
        out << YAML::Key << "offset" << YAML::Value << pc.Offset;
        out << YAML::Key << "size"   << YAML::Value << pc.Size;
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

void PipelineWindow::DrawFileControls(const FS::FileSystemManager& fileSystem)
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

void PipelineWindow::DrawDescriptorSets()
{
    if (!ImGui::CollapsingHeader("Descriptor Sets", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::TextDisabled("Set index = position in list. Removing a set shifts all subsequent indices.");
    ImGui::Spacing();

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingStretchProp;

    int toRemoveSet = -1;

    for (int si = 0; si < static_cast<int>(m_Sets.size()); ++si)
    {
        auto& set = m_Sets[si];
        ImGui::PushID(si);

        // X button before the tree node so it sits in the same header line
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
        if (ImGui::SmallButton("X")) toRemoveSet = si;
        ImGui::PopStyleColor(2);

        ImGui::SameLine();

        const bool setOpen = ImGui::TreeNodeEx(
            "##set_node",
            ImGuiTreeNodeFlags_DefaultOpen,
            "Set %d  (%zu binding%s)",
            si,
            set.Bindings.size(),
            set.Bindings.size() == 1 ? "" : "s");

        if (setOpen)
        {
            if (ImGui::BeginTable("##bind_tbl", 8, tableFlags))
            {
                ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed,   28.0f);
                ImGui::TableSetupColumn("Binding",  ImGuiTableColumnFlags_WidthFixed,   65.0f);
                ImGui::TableSetupColumn("Type",     ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Count",    ImGuiTableColumnFlags_WidthFixed,   50.0f);
                ImGui::TableSetupColumn("V",        ImGuiTableColumnFlags_WidthFixed,   24.0f);
                ImGui::TableSetupColumn("F",        ImGuiTableColumnFlags_WidthFixed,   24.0f);
                ImGui::TableSetupColumn("C",        ImGuiTableColumnFlags_WidthFixed,   24.0f);
                ImGui::TableSetupColumn("##del",    ImGuiTableColumnFlags_WidthFixed,   26.0f);
                ImGui::TableHeadersRow();

                int toDeleteBinding = -1;
                for (int bi = 0; bi < static_cast<int>(set.Bindings.size()); ++bi)
                {
                    auto& b = set.Bindings[bi];
                    const bool dupBinding = HasDuplicateBinding(si, b.Binding, bi);
                    const bool noStage    = NoStagesSelected(b.Vertex, b.Fragment, b.Compute);

                    ImGui::TableNextRow();
                    ImGui::PushID(bi);

                    // Row index
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", bi);

                    // Binding index — red on duplicate
                    ImGui::TableSetColumnIndex(1);
                    if (dupBinding)
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##bnd", &b.Binding))
                    {
                        b.Binding = std::max(0, b.Binding);
                        m_Dirty = true;
                    }
                    if (dupBinding) ImGui::PopStyleColor();

                    // Type dropdown
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("##type", &b.Type, k_TypeDisplayNames, k_TypeCount))
                        m_Dirty = true;

                    // Count
                    ImGui::TableSetColumnIndex(3);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##cnt", &b.Count))
                    {
                        b.Count = std::max(1, b.Count);
                        m_Dirty = true;
                    }

                    // Stage checkboxes — orange tint when none selected
                    if (noStage)
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));

                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::Checkbox("##v", &b.Vertex))   m_Dirty = true;
                    ImGui::TableSetColumnIndex(5);
                    if (ImGui::Checkbox("##f", &b.Fragment)) m_Dirty = true;
                    ImGui::TableSetColumnIndex(6);
                    if (ImGui::Checkbox("##c", &b.Compute))  m_Dirty = true;

                    if (noStage) ImGui::PopStyleColor();

                    // Delete
                    ImGui::TableSetColumnIndex(7);
                    if (ImGui::SmallButton("X")) toDeleteBinding = bi;

                    ImGui::PopID();
                }
                ImGui::EndTable();

                if (toDeleteBinding >= 0)
                {
                    set.Bindings.erase(set.Bindings.begin() + toDeleteBinding);
                    m_Dirty = true;
                }
            }

            if (ImGui::Button("+ Add binding"))
            {
                BindingState b;
                b.Binding = static_cast<int>(set.Bindings.size());
                b.Vertex  = true;
                set.Bindings.push_back(b);
                m_Dirty = true;
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    if (toRemoveSet >= 0)
    {
        m_Sets.erase(m_Sets.begin() + toRemoveSet);
        m_Dirty = true;
    }

    ImGui::Spacing();
    if (ImGui::Button("+ Add set"))
    {
        m_Sets.push_back(SetState{});
        m_Dirty = true;
    }
}

void PipelineWindow::DrawPushConstants()
{
    if (!ImGui::CollapsingHeader("Push Constants", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##pc_tbl", 8, tableFlags))
    {
        ImGui::TableSetupColumn("#",       ImGuiTableColumnFlags_WidthFixed,   28.0f);
        ImGui::TableSetupColumn("Offset",  ImGuiTableColumnFlags_WidthFixed,   65.0f);
        ImGui::TableSetupColumn("Size",    ImGuiTableColumnFlags_WidthFixed,   65.0f);
        ImGui::TableSetupColumn("V",       ImGuiTableColumnFlags_WidthFixed,   24.0f);
        ImGui::TableSetupColumn("F",       ImGuiTableColumnFlags_WidthFixed,   24.0f);
        ImGui::TableSetupColumn("C",       ImGuiTableColumnFlags_WidthFixed,   24.0f);
        ImGui::TableSetupColumn("Range",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##del",   ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableHeadersRow();

        int toDelete = -1;
        for (int i = 0; i < static_cast<int>(m_PushConstants.size()); ++i)
        {
            auto& pc = m_PushConstants[i];
            const bool noStage  = NoStagesSelected(pc.Vertex, pc.Fragment, pc.Compute);
            const bool zeroSize = (pc.Size <= 0);

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // Row index
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            // Offset
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##off", &pc.Offset))
            {
                pc.Offset = std::max(0, pc.Offset);
                m_Dirty = true;
            }

            // Size — red on zero
            ImGui::TableSetColumnIndex(2);
            if (zeroSize)
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##sz", &pc.Size))
            {
                pc.Size = std::max(0, pc.Size);
                m_Dirty = true;
            }
            if (zeroSize) ImGui::PopStyleColor();

            // Stage checkboxes — orange tint when none selected
            if (noStage)
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));

            ImGui::TableSetColumnIndex(3);
            if (ImGui::Checkbox("##v", &pc.Vertex))   m_Dirty = true;
            ImGui::TableSetColumnIndex(4);
            if (ImGui::Checkbox("##f", &pc.Fragment)) m_Dirty = true;
            ImGui::TableSetColumnIndex(5);
            if (ImGui::Checkbox("##c", &pc.Compute))  m_Dirty = true;

            if (noStage) ImGui::PopStyleColor();

            // Byte range summary
            ImGui::TableSetColumnIndex(6);
            if (pc.Size > 0)
                ImGui::TextDisabled("[%d, %d)", pc.Offset, pc.Offset + pc.Size);
            else
                ImGui::TextDisabled("--");

            // Delete
            ImGui::TableSetColumnIndex(7);
            if (ImGui::SmallButton("X")) toDelete = i;

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (toDelete >= 0)
        {
            m_PushConstants.erase(m_PushConstants.begin() + toDelete);
            m_Dirty = true;
        }
    }

    if (ImGui::Button("+ Add range"))
    {
        m_PushConstants.push_back(PushConstantState{});
        m_Dirty = true;
    }
}

void PipelineWindow::DrawValidation()
{
    const bool empty = m_Sets.empty() && m_PushConstants.empty();
    if (empty)
    {
        ImGui::TextDisabled("No data. Add descriptor sets and push constant ranges above.");
        return;
    }

    bool anyIssue = false;

    // ── Descriptor sets ───────────────────────────────────────────────────────

    for (int si = 0; si < static_cast<int>(m_Sets.size()); ++si)
    {
        const auto& set = m_Sets[si];

        if (set.Bindings.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
            ImGui::Text("[warn]  Set %d has no bindings", si);
            ImGui::PopStyleColor();
            anyIssue = true;
        }

        // Duplicate binding indices
        std::set<int> reportedDups;
        for (int bi = 0; bi < static_cast<int>(set.Bindings.size()); ++bi)
        {
            const int idx = set.Bindings[bi].Binding;
            if (HasDuplicateBinding(si, idx, bi) && reportedDups.insert(idx).second)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("[error] Set %d: duplicate binding index %d", si, idx);
                ImGui::PopStyleColor();
                anyIssue = true;
            }
        }

        // No stages on a binding
        for (int bi = 0; bi < static_cast<int>(set.Bindings.size()); ++bi)
        {
            const auto& b = set.Bindings[bi];
            if (NoStagesSelected(b.Vertex, b.Fragment, b.Compute))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("[error] Set %d, binding %d: no shader stages selected", si, b.Binding);
                ImGui::PopStyleColor();
                anyIssue = true;
            }
        }
    }

    // ── Push constants ────────────────────────────────────────────────────────

    for (int i = 0; i < static_cast<int>(m_PushConstants.size()); ++i)
    {
        const auto& pc = m_PushConstants[i];

        if (pc.Size <= 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("[error] Push constant range %d: size must be > 0", i);
            ImGui::PopStyleColor();
            anyIssue = true;
        }

        if (NoStagesSelected(pc.Vertex, pc.Fragment, pc.Compute))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("[error] Push constant range %d: no shader stages selected", i);
            ImGui::PopStyleColor();
            anyIssue = true;
        }

        // Overlap check: report each pair once
        for (int j = i + 1; j < static_cast<int>(m_PushConstants.size()); ++j)
        {
            if (PushConstantsOverlap(i, j))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
                ImGui::Text("[warn]  Push constant ranges %d and %d overlap in byte range", i, j);
                ImGui::PopStyleColor();
                anyIssue = true;
            }
        }
    }

    if (!anyIssue)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 0.4f, 1.0f));
        ImGui::Text("OK");
        ImGui::PopStyleColor();
    }
}

void PipelineWindow::Draw(const FS::FileSystemManager& fileSystem)
{
    if (!Open)
        return;

    const std::string title =
        std::string("Pipeline") + (m_Dirty ? " *" : "") + "###Pipeline";

    ImGui::SetNextWindowSize(ImVec2(680.0f, 580.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(title.c_str(), &Open))
    {
        ImGui::End();
        return;
    }

    DrawFileControls(fileSystem);
    ImGui::Spacing();
    DrawDescriptorSets();
    ImGui::Spacing();
    DrawPushConstants();
    ImGui::Spacing();
    ImGui::SeparatorText("Validation");
    DrawValidation();

    ImGui::End();
}

} // namespace Editor
