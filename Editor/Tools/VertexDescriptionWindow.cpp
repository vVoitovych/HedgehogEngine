#include "VertexDescriptionWindow.hpp"

#include "DialogueWindows/api/VertexDescDialogue.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <set>

namespace Editor
{

namespace
{
    constexpr const char* k_FormatNames[] = {
        "r32_float",
        "r32g32_float",
        "r32g32b32_float",
        "r32g32b32a32_float",
    };
    constexpr int k_FormatCount = 4;

    constexpr const char* k_InputRateNames[] = {
        "per_vertex",
        "per_instance",
    };
    constexpr int k_InputRateCount = 2;
}

// ── Format / input-rate helpers ───────────────────────────────────────────────

int VertexDescriptionWindow::FormatToIndex(const std::string& fmt)
{
    for (int i = 0; i < k_FormatCount; ++i)
        if (fmt == k_FormatNames[i])
            return i;
    return 2; // fallback: r32g32b32_float
}

const char* VertexDescriptionWindow::IndexToFormat(int idx)
{
    return (idx >= 0 && idx < k_FormatCount) ? k_FormatNames[idx] : k_FormatNames[2];
}

const char* VertexDescriptionWindow::IndexToInputRate(int idx)
{
    return (idx >= 0 && idx < k_InputRateCount) ? k_InputRateNames[idx] : k_InputRateNames[0];
}

// ── Validation helpers ────────────────────────────────────────────────────────

bool VertexDescriptionWindow::HasDuplicateBindingIndex(int index, int skipRow) const
{
    for (int i = 0; i < static_cast<int>(m_Bindings.size()); ++i)
        if (i != skipRow && m_Bindings[i].m_Binding == index)
            return true;
    return false;
}

bool VertexDescriptionWindow::HasDuplicateAttributeLocation(int location, int skipRow) const
{
    for (int i = 0; i < static_cast<int>(m_Attributes.size()); ++i)
        if (i != skipRow && m_Attributes[i].m_Location == location)
            return true;
    return false;
}

bool VertexDescriptionWindow::AttributeBindingExists(int binding) const
{
    for (const auto& b : m_Bindings)
        if (b.m_Binding == binding)
            return true;
    return false;
}

// ── File I/O ──────────────────────────────────────────────────────────────────

void VertexDescriptionWindow::NewFile()
{
    m_FilePath.clear();
    m_VirtualPath.clear();
    m_Bindings.clear();
    m_Attributes.clear();
    m_Dirty = false;
}

void VertexDescriptionWindow::OpenFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::VertexDescOpenDialogue();
    if (path != nullptr)
        LoadFromPath(path, fileSystem);
}

void VertexDescriptionWindow::SaveFile(const FS::FileSystemManager& fileSystem)
{
    if (m_VirtualPath.empty())
        SaveAsFile(fileSystem);
    else
        SaveToPath(m_VirtualPath, fileSystem);
}

void VertexDescriptionWindow::SaveAsFile(const FS::FileSystemManager& fileSystem)
{
    char* path = DialogueWindows::VertexDescSaveDialogue();
    if (path != nullptr)
    {
        const auto virtualPath = fileSystem.ToVirtualPath(path);
        if (!virtualPath)
        {
            LOGERROR("VertexDescriptionWindow: '", path, "' is outside all mounted roots and cannot be saved.");
            return;
        }
        m_FilePath    = path;
        m_VirtualPath = *virtualPath;
        SaveToPath(m_VirtualPath, fileSystem);
    }
}

bool VertexDescriptionWindow::LoadFromPath(const std::string& path,
                                            const FS::FileSystemManager& fileSystem)
{
    const auto virtualPath = fileSystem.ToVirtualPath(path);
    if (!virtualPath)
    {
        LOGERROR("VertexDescriptionWindow: '", path, "' is outside all mounted roots and cannot be opened.");
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

    m_Bindings.clear();
    m_Attributes.clear();

    if (const YAML::Node& bn = root["bindings"])
    {
        for (const YAML::Node& b : bn)
        {
            BindingState bs;
            bs.m_Binding   = b["binding"].as<int>(0);
            bs.m_Stride    = b["stride"].as<int>(12);
            const std::string rate = b["input_rate"] ? b["input_rate"].as<std::string>() : "per_vertex";
            bs.m_InputRate = (rate == "per_instance") ? 1 : 0;
            m_Bindings.push_back(bs);
        }
    }

    if (const YAML::Node& an = root["attributes"])
    {
        for (const YAML::Node& a : an)
        {
            AttributeState as;
            as.m_Location = a["location"].as<int>(0);
            as.m_Binding  = a["binding"].as<int>(0);
            as.m_Format   = FormatToIndex(a["format"] ? a["format"].as<std::string>() : "r32g32b32_float");
            as.m_Offset   = a["offset"].as<int>(0);
            m_Attributes.push_back(as);
        }
    }

    m_FilePath    = path;
    m_VirtualPath = *virtualPath;
    m_Dirty       = false;
    return true;
}

bool VertexDescriptionWindow::SaveToPath(const std::string& virtualPath,
                                          const FS::FileSystemManager& fileSystem)
{
    if (virtualPath.empty())
    {
        LOGERROR("VertexDescriptionWindow: cannot save — file path is outside the engine root.");
        return false;
    }

    YAML::Emitter out;
    out << YAML::BeginMap;

    out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;
    for (const auto& b : m_Bindings)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "binding"    << YAML::Value << b.m_Binding;
        out << YAML::Key << "stride"     << YAML::Value << b.m_Stride;
        out << YAML::Key << "input_rate" << YAML::Value << IndexToInputRate(b.m_InputRate);
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::Key << "attributes" << YAML::Value << YAML::BeginSeq;
    for (const auto& a : m_Attributes)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "location" << YAML::Value << a.m_Location;
        out << YAML::Key << "binding"  << YAML::Value << a.m_Binding;
        out << YAML::Key << "format"   << YAML::Value << IndexToFormat(a.m_Format);
        out << YAML::Key << "offset"   << YAML::Value << a.m_Offset;
        out << YAML::EndMap;
    }
    out << YAML::EndSeq;

    out << YAML::EndMap;

    if (!fileSystem.WriteTextFile(virtualPath, out.c_str()))
        return false;

    m_Dirty = false;
    return true;
}

// ── UI ────────────────────────────────────────────────────────────────────────

void VertexDescriptionWindow::DrawFileControls(const FS::FileSystemManager& fileSystem)
{
    if (ImGui::Button("New"))     NewFile();
    ImGui::SameLine();
    if (ImGui::Button("Open"))    OpenFile(fileSystem);
    ImGui::SameLine();
    if (ImGui::Button("Save"))    SaveFile(fileSystem);
    ImGui::SameLine();
    if (ImGui::Button("Save As")) SaveAsFile(fileSystem);

    ImGui::Separator();

    const std::string displayPath = m_FilePath.empty() ? "(unsaved)" : m_FilePath;
    ImGui::TextDisabled("File: %s%s", displayPath.c_str(), m_Dirty ? "  *" : "");
}

void VertexDescriptionWindow::DrawBindingsTable()
{
    if (!ImGui::CollapsingHeader("Bindings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##bind_tbl", 5, flags))
    {
        ImGui::TableSetupColumn("#",          ImGuiTableColumnFlags_WidthFixed,   28.0f);
        ImGui::TableSetupColumn("Binding",    ImGuiTableColumnFlags_WidthFixed,   70.0f);
        ImGui::TableSetupColumn("Stride",     ImGuiTableColumnFlags_WidthFixed,   70.0f);
        ImGui::TableSetupColumn("Input Rate", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##del",      ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableHeadersRow();

        int toDelete = -1;
        for (int i = 0; i < static_cast<int>(m_Bindings.size()); ++i)
        {
            auto& b = m_Bindings[i];
            const bool dupIdx = HasDuplicateBindingIndex(b.m_Binding, i);

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // Row index label
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            // Binding index — red background when duplicate
            ImGui::TableSetColumnIndex(1);
            if (dupIdx) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##b", &b.m_Binding))
            {
                b.m_Binding = std::max(0, b.m_Binding);
                m_Dirty = true;
            }
            if (dupIdx) ImGui::PopStyleColor();

            // Stride
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##s", &b.m_Stride))
            {
                b.m_Stride = std::max(1, b.m_Stride);
                m_Dirty = true;
            }

            // Input rate
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##ir", &b.m_InputRate, k_InputRateNames, k_InputRateCount))
                m_Dirty = true;

            // Delete
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("X"))
                toDelete = i;

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (toDelete >= 0)
        {
            m_Bindings.erase(m_Bindings.begin() + toDelete);
            m_Dirty = true;
        }
    }

    if (ImGui::Button("+ Add binding"))
    {
        BindingState b;
        b.m_Binding = static_cast<int>(m_Bindings.size());
        m_Bindings.push_back(b);
        m_Dirty = true;
    }
}

void VertexDescriptionWindow::DrawAttributesTable()
{
    if (!ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders       |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##attr_tbl", 6, flags))
    {
        ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed,   28.0f);
        ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed,   70.0f);
        ImGui::TableSetupColumn("Binding",  ImGuiTableColumnFlags_WidthFixed,   65.0f);
        ImGui::TableSetupColumn("Format",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Offset",   ImGuiTableColumnFlags_WidthFixed,   60.0f);
        ImGui::TableSetupColumn("##del",    ImGuiTableColumnFlags_WidthFixed,   26.0f);
        ImGui::TableHeadersRow();

        int toDelete = -1;
        for (int i = 0; i < static_cast<int>(m_Attributes.size()); ++i)
        {
            auto& a = m_Attributes[i];
            const bool dupLoc        = HasDuplicateAttributeLocation(a.m_Location, i);
            const bool missingBinding = !AttributeBindingExists(a.m_Binding);

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // Row index label
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", i);

            // Location — red when duplicate
            ImGui::TableSetColumnIndex(1);
            if (dupLoc) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.55f, 0.1f, 0.1f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##loc", &a.m_Location))
            {
                a.m_Location = std::max(0, a.m_Location);
                m_Dirty = true;
            }
            if (dupLoc) ImGui::PopStyleColor();

            // Binding ref — orange when it doesn't exist in bindings table
            ImGui::TableSetColumnIndex(2);
            if (missingBinding) ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.28f, 0.0f, 1.0f));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##ab", &a.m_Binding))
            {
                a.m_Binding = std::max(0, a.m_Binding);
                m_Dirty = true;
            }
            if (missingBinding) ImGui::PopStyleColor();

            // Format dropdown
            ImGui::TableSetColumnIndex(3);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##fmt", &a.m_Format, k_FormatNames, k_FormatCount))
                m_Dirty = true;

            // Offset
            ImGui::TableSetColumnIndex(4);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputInt("##off", &a.m_Offset))
            {
                a.m_Offset = std::max(0, a.m_Offset);
                m_Dirty = true;
            }

            // Delete
            ImGui::TableSetColumnIndex(5);
            if (ImGui::SmallButton("X"))
                toDelete = i;

            ImGui::PopID();
        }
        ImGui::EndTable();

        if (toDelete >= 0)
        {
            m_Attributes.erase(m_Attributes.begin() + toDelete);
            m_Dirty = true;
        }
    }

    if (ImGui::Button("+ Add attribute"))
    {
        AttributeState a;
        a.m_Location = static_cast<int>(m_Attributes.size());
        m_Attributes.push_back(a);
        m_Dirty = true;
    }
}

void VertexDescriptionWindow::DrawValidation()
{
    if (m_Bindings.empty() && m_Attributes.empty())
    {
        ImGui::TextDisabled("No data. Use \"+ Add binding\" and \"+ Add attribute\" above.");
        return;
    }

    bool anyIssue = false;

    // Report each duplicate binding index value once
    std::set<int> reportedBindings;
    for (int i = 0; i < static_cast<int>(m_Bindings.size()); ++i)
    {
        const int idx = m_Bindings[i].m_Binding;
        if (HasDuplicateBindingIndex(idx, i) && reportedBindings.insert(idx).second)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("[error] Duplicate binding index %d", idx);
            ImGui::PopStyleColor();
            anyIssue = true;
        }
    }

    // Report each duplicate attribute location once
    std::set<int> reportedLocations;
    for (int i = 0; i < static_cast<int>(m_Attributes.size()); ++i)
    {
        const int loc = m_Attributes[i].m_Location;
        if (HasDuplicateAttributeLocation(loc, i) && reportedLocations.insert(loc).second)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            ImGui::Text("[error] Duplicate attribute location %d", loc);
            ImGui::PopStyleColor();
            anyIssue = true;
        }
    }

    // Report attributes referencing undefined bindings
    for (const auto& a : m_Attributes)
    {
        if (!AttributeBindingExists(a.m_Binding))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.0f, 1.0f));
            ImGui::Text("[warn]  Attribute location %d references undefined binding %d",
                a.m_Location, a.m_Binding);
            ImGui::PopStyleColor();
            anyIssue = true;
        }
    }

    if (!anyIssue)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.85f, 0.4f, 1.0f));
        ImGui::Text("OK");
        ImGui::PopStyleColor();
    }
}

void VertexDescriptionWindow::Draw(const FS::FileSystemManager& fileSystem)
{
    if (!m_Open)
        return;

    // Stable window identity via ###; title prefix changes with dirty state
    const std::string title =
        std::string("Vertex Descriptions") + (m_Dirty ? " *" : "") + "###VertexDescriptions";

    ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin(title.c_str(), &m_Open))
    {
        ImGui::End();
        return;
    }

    DrawFileControls(fileSystem);
    ImGui::Spacing();
    DrawBindingsTable();
    ImGui::Spacing();
    DrawAttributesTable();
    ImGui::Spacing();
    ImGui::SeparatorText("Validation");
    DrawValidation();

    ImGui::End();
}

} // namespace Editor
