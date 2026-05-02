#include "PipelineWindow.hpp"

#include "DialogueWindows/api/PipelineDialogue.hpp"

#include "imgui.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <fstream>
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
    const auto& bindings = m_Sets[setIdx].m_Bindings;
    for (int i = 0; i < static_cast<int>(bindings.size()); ++i)
        if (i != skipRow && bindings[i].m_Binding == binding)
            return true;
    return false;
}

bool PipelineWindow::PushConstantsOverlap(int i, int j) const
{
    const auto& a = m_PushConstants[i];
    const auto& b = m_PushConstants[j];
    const int aEnd = a.m_Offset + a.m_Size;
    const int bEnd = b.m_Offset + b.m_Size;
    return (a.m_Offset < bEnd) && (b.m_Offset < aEnd);
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void PipelineWindow::NewFile()
{
    m_FilePath.clear();
    m_Sets.clear();
    m_PushConstants.clear();
    m_Dirty = false;
}

void PipelineWindow::OpenFile()
{
    char* path = DialogueWindows::PipelineOpenDialogue();
    if (path != nullptr)
        LoadFromPath(path);
}

void PipelineWindow::SaveFile()
{
    if (m_FilePath.empty())
        SaveAsFile();
    else
        SaveToPath(m_FilePath);
}

void PipelineWindow::SaveAsFile()
{
    char* path = DialogueWindows::PipelineSaveDialogue();
    if (path != nullptr)
    {
        m_FilePath = path;
        SaveToPath(m_FilePath);
    }
}

bool PipelineWindow::LoadFromPath(const std::string& path)
{
    YAML::Node root;
    try
    {
        root = YAML::LoadFile(path);
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
                    bs.m_Binding = b["binding"].as<int>(0);
                    bs.m_Type    = TypeNameToIndex(b["type"] ? b["type"].as<std::string>() : "uniform_buffer");
                    bs.m_Count   = b["count"].as<int>(1);
                    ParseStageString(b["stage"] ? b["stage"].as<std::string>() : "vertex",
                                     bs.m_Vertex, bs.m_Fragment, bs.m_Compute);
                    set.m_Bindings.push_back(bs);
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
            ps.m_Offset = pc["offset"].as<int>(0);
            ps.m_Size   = pc["size"].as<int>(0);
            ParseStageString(pc["stage"] ? pc["stage"].as<std::string>() : "vertex",
                             ps.m_Vertex, ps.m_Fragment, ps.m_Compute);
            m_PushConstants.push_back(ps);
        }
    }

    m_FilePath = path;
    m_Dirty    = false;
    return true;
}

bool PipelineWindow::SaveToPath(const std::string& path)
{
    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "descriptor_sets" << YAML::Value << YAML::BeginSeq;
    for (const auto& set : m_Sets)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
        for (const auto& b : set.m_Bindings)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "binding" << YAML::Value << b.m_Binding;
            out << YAML::Key << "type"    << YAML::Value << IndexToTypeName(b.m_Type);
            out << YAML::Key << "stage"   << YAML::Value << StagesToString(b.m_Vertex, b.m_Fragment, b.m_Compute);
            out << YAML::Key << "count"   << YAML::Value << b.m_Count;
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
        out << YAML::Key << "stage"  << YAML::Value << StagesToString(pc.m_Vertex, pc.m_Fragment, pc.m_Compute);
        out << YAML::Key << "offset" << YAML::Value << pc.m_Offset;
        out << YAML::Key << "size"   << YAML::Value << pc.m_Size;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;

    std::ofstream file(path);
    if (!file.is_open())
        return false;

    file << out.c_str();
    m_Dirty = false;
    return true;
}

// ── UI sections ───────────────────────────────────────────────────────────────

void PipelineWindow::DrawFileControls()
{
    if (ImGui::Button("New"))     NewFile();
    ImGui::SameLine();
    if (ImGui::Button("Open"))    OpenFile();
    ImGui::SameLine();
    if (ImGui::Button("Save"))    SaveFile();
    ImGui::SameLine();
    if (ImGui::Button("Save As")) SaveAsFile();

    ImGui::Separator();

    const std::string display = m_FilePath.empty() ? "(unsaved)" : m_FilePath;
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
            set.m_Bindings.size(),
            set.m_Bindings.size() == 1 ? "" : "s");

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
                for (int bi = 0; bi < static_cast<int>(set.m_Bindings.size()); ++bi)
                {
                    auto& b = set.m_Bindings[bi];
                    const bool dupBinding = HasDuplicateBinding(si, b.m_Binding, bi);
                    const bool noStage    = NoStagesSelected(b.m_Vertex, b.m_Fragment, b.m_Compute);

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
                    if (ImGui::InputInt("##bnd", &b.m_Binding))
                    {
                        b.m_Binding = std::max(0, b.m_Binding);
                        m_Dirty = true;
                    }
                    if (dupBinding) ImGui::PopStyleColor();

                    // Type dropdown
                    ImGui::TableSetColumnIndex(2);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::Combo("##type", &b.m_Type, k_TypeDisplayNames, k_TypeCount))
                        m_Dirty = true;

                    // Count
                    ImGui::TableSetColumnIndex(3);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##cnt", &b.m_Count))
                    {
                        b.m_Count = std::max(1, b.m_Count);
                        m_Dirty = true;
                    }

                    // Stage checkboxes — orange tint when none selected
                    if (noStage)
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));

                    ImGui::TableSetColumnIndex(4);
                    if (ImGui::Checkbox("##v", &b.m_Vertex))   m_Dirty = true;
                    ImGui::TableSetColumnIndex(5);
                    if (ImGui::Checkbox("##f", &b.m_Fragment)) m_Dirty = true;
                    ImGui::TableSetColumnIndex(6);
                    if (ImGui::Checkbox("##c", &b.m_Compute))  m_Dirty = true;

                    if (noStage) ImGui::PopStyleColor();

                    // Delete
                    ImGui::TableSetColumnIndex(7);
                    if (ImGui::SmallButton("X")) toDeleteBinding = bi;

                    ImGui::PopID();
                }
                ImGui::EndTable();

                if (toDeleteBinding >= 0)
                {
                    set.m_Bindings.erase(set.m_Bindings.begin() + toDeleteBinding);
                    m_Dirty = true;
                }
            }

            if (ImGui::Button("+ Add binding"))
            {
                BindingState b;
                b.m_Binding = static_cast<int>(set.m_Bindings.size());
                b.m_Vertex  = true;
                set.m_Bindings.push_back(b);
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
            const bool noStage  = NoStagesSelected(pc.m_Vertex, pc.m_Fragment, pc.m_Compute);
            const bool zeroSize = (pc.m_Size <= 0);

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // Row index
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            // Offset
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##off", &pc.m_Offset))
            {
                pc.m_Offset = std::max(0, pc.m_Offset);
                m_Dirty = true;
            }

            // Size — red on zero
            ImGui::TableSetColumnIndex(2);
            if (zeroSize)
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##sz", &pc.m_Size))
            {
                pc.m_Size = std::max(0, pc.m_Size);
                m_Dirty = true;
            }
            if (zeroSize) ImGui::PopStyleColor();

            // Stage checkboxes — orange tint when none selected
            if (noStage)
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));

            ImGui::TableSetColumnIndex(3);
            if (ImGui::Checkbox("##v", &pc.m_Vertex))   m_Dirty = true;
            ImGui::TableSetColumnIndex(4);
            if (ImGui::Checkbox("##f", &pc.m_Fragment)) m_Dirty = true;
            ImGui::TableSetColumnIndex(5);
            if (ImGui::Checkbox("##c", &pc.m_Compute))  m_Dirty = true;

            if (noStage) ImGui::PopStyleColor();

            // Byte range summary
            ImGui::TableSetColumnIndex(6);
            if (pc.m_Size > 0)
                ImGui::TextDisabled("[%d, %d)", pc.m_Offset, pc.m_Offset + pc.m_Size);
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

        if (set.m_Bindings.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
            ImGui::Text("[warn]  Set %d has no bindings", si);
            ImGui::PopStyleColor();
            anyIssue = true;
        }

        // Duplicate binding indices
        std::set<int> reportedDups;
        for (int bi = 0; bi < static_cast<int>(set.m_Bindings.size()); ++bi)
        {
            const int idx = set.m_Bindings[bi].m_Binding;
            if (HasDuplicateBinding(si, idx, bi) && reportedDups.insert(idx).second)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("[error] Set %d: duplicate binding index %d", si, idx);
                ImGui::PopStyleColor();
                anyIssue = true;
            }
        }

        // No stages on a binding
        for (int bi = 0; bi < static_cast<int>(set.m_Bindings.size()); ++bi)
        {
            const auto& b = set.m_Bindings[bi];
            if (NoStagesSelected(b.m_Vertex, b.m_Fragment, b.m_Compute))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::Text("[error] Set %d, binding %d: no shader stages selected", si, b.m_Binding);
                ImGui::PopStyleColor();
                anyIssue = true;
            }
        }
    }

    // ── Push constants ────────────────────────────────────────────────────────

    for (int i = 0; i < static_cast<int>(m_PushConstants.size()); ++i)
    {
        const auto& pc = m_PushConstants[i];

        if (pc.m_Size <= 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("[error] Push constant range %d: size must be > 0", i);
            ImGui::PopStyleColor();
            anyIssue = true;
        }

        if (NoStagesSelected(pc.m_Vertex, pc.m_Fragment, pc.m_Compute))
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

void PipelineWindow::Draw()
{
    if (!m_Open)
        return;

    const std::string title =
        std::string("Pipeline") + (m_Dirty ? " *" : "") + "###Pipeline";

    ImGui::SetNextWindowSize(ImVec2(680.0f, 580.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::End();
        return;
    }

    DrawFileControls();
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
