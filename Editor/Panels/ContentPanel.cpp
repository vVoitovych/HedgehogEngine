#include "ContentPanel.hpp"

#include "ContentTypes.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "imgui.h"

#include <algorithm>
#include <cctype>

namespace Editor
{
    namespace
    {
        constexpr const char* ROOT_FOLDER = "assets://";
        constexpr const char* ROOT_LABEL  = "Assets";

        // A file added while the editor runs shows within this long.
        constexpr std::chrono::milliseconds REFRESH_INTERVAL{ 1000 };

        constexpr float TREE_WIDTH   = 200.0f;
        constexpr float SEARCH_WIDTH = 180.0f;
        constexpr float CELL_WIDTH   = 84.0f;
        constexpr float ICON_SIZE    = 56.0f;
        constexpr float CELL_PADDING = 6.0f;

        std::string JoinPath(const std::string& folder, const std::string& name)
        {
            return folder.ends_with("://") ? folder + name : folder + "/" + name;
        }

        bool ContainsIgnoringCase(std::string_view text, std::string_view pattern)
        {
            const auto equal = [](char a, char b)
            {
                return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            };
            return std::ranges::search(text, pattern, equal).begin() != text.end() || pattern.empty();
        }

        // The longest start of text that fits in width, with "..." when cut.
        std::string FitToWidth(const std::string& text, float width)
        {
            if (ImGui::CalcTextSize(text.c_str()).x <= width)
                return text;
            std::string cut = text;
            while (!cut.empty() && ImGui::CalcTextSize((cut + "...").c_str()).x > width)
                cut.pop_back();
            return cut + "...";
        }

        ImU32 IconColor(ContentType type)
        {
            switch (type)
            {
            case ContentType::Folder:            return IM_COL32(214, 170, 72, 255);
            case ContentType::Scene:             return IM_COL32(86, 156, 214, 255);
            case ContentType::Material:          return IM_COL32(197, 108, 192, 255);
            case ContentType::Texture:           return IM_COL32(96, 180, 110, 255);
            case ContentType::Mesh:              return IM_COL32(220, 130, 70, 255);
            case ContentType::Script:            return IM_COL32(120, 120, 220, 255);
            case ContentType::Shader:
            case ContentType::Pipeline:
            case ContentType::VertexDescription: return IM_COL32(70, 170, 170, 255);
            case ContentType::RenderGraph:       return IM_COL32(200, 90, 90, 255);
            case ContentType::Other:             return IM_COL32(120, 120, 120, 255);
            }
            return IM_COL32(120, 120, 120, 255);
        }
    }

    ContentPanel::ContentPanel(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
        , m_Current(ROOT_FOLDER)
    {
    }

    void ContentPanel::Draw()
    {
        // A folder deleted while it was shown: fall back to the root.
        if (!GetListing(m_Current).Exists && m_Current != ROOT_FOLDER)
            Navigate(ROOT_FOLDER);

        ImGui::BeginChild("ContentTree", ImVec2(TREE_WIDTH, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawFolderTree(ROOT_FOLDER, ROOT_LABEL);
        m_RevealCurrent = false;
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginGroup();
        DrawBreadcrumb();
        ImGui::BeginChild("ContentGrid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        DrawGrid();
        ImGui::EndChild();
        ImGui::EndGroup();
    }

    const ContentPanel::Listing& ContentPanel::GetListing(const std::string& folder)
    {
        const auto now = std::chrono::steady_clock::now();
        auto [it, inserted] = m_Listings.try_emplace(folder);
        Listing& listing = it->second;
        if (inserted || now - listing.Scanned >= REFRESH_INTERVAL)
        {
            auto entries    = m_FileSystem.ListDirectory(folder);
            listing.Exists  = entries.has_value();
            listing.Entries = entries ? std::move(*entries) : std::vector<FS::DirectoryEntry>{};
            listing.Scanned = now;
        }
        return listing;
    }

    void ContentPanel::Navigate(const std::string& folder)
    {
        m_Current = folder;
        m_Selected.clear();
        m_Search[0] = '\0'; // the search filters one folder; another starts unfiltered
        m_Listings.erase(folder); // read it now, not up to a second from now
        m_RevealCurrent = true;
    }

    void ContentPanel::DrawFolderTree(const std::string& folder, const std::string& label)
    {
        // Copied: drawing the children reads their listings, which may rescan this one's map entry.
        std::vector<std::string> subfolders;
        for (const FS::DirectoryEntry& entry : GetListing(folder).Entries)
        {
            if (entry.IsDirectory)
                subfolders.push_back(entry.Name);
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                                 | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (subfolders.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (folder == m_Current)
            flags |= ImGuiTreeNodeFlags_Selected;
        if (folder == ROOT_FOLDER)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        // Open the folders above the current one after navigating from the grid or breadcrumb.
        const bool above = folder == ROOT_FOLDER ? m_Current != folder : m_Current.starts_with(folder + "/");
        if (m_RevealCurrent && above)
            ImGui::SetNextItemOpen(true);

        const bool open = ImGui::TreeNodeEx(folder.c_str(), flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            Navigate(folder);
        if (!open)
            return;
        for (const std::string& name : subfolders)
            DrawFolderTree(JoinPath(folder, name), name);
        ImGui::TreePop();
    }

    void ContentPanel::DrawBreadcrumb()
    {
        // "Assets > Models > Default", each part a button that goes there.
        std::string target = ROOT_FOLDER;
        if (ImGui::SmallButton(ROOT_LABEL))
            Navigate(ROOT_FOLDER);

        const std::string relative = m_Current.substr(std::string_view(ROOT_FOLDER).size());
        size_t start = 0;
        int    part  = 0;
        while (start < relative.size())
        {
            const size_t      end  = std::min(relative.find('/', start), relative.size());
            const std::string name = relative.substr(start, end - start);
            target = JoinPath(target, name);
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
            ImGui::PushID(part++);
            if (ImGui::SmallButton(name.c_str()))
                Navigate(target);
            ImGui::PopID();
            start = end + 1;
        }

        // The search box at the row's right end, or right after the breadcrumb when that is long.
        ImGui::SameLine();
        const float spare = ImGui::GetContentRegionAvail().x - SEARCH_WIDTH;
        if (spare > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spare);
        ImGui::SetNextItemWidth(SEARCH_WIDTH);
        ImGui::InputTextWithHint("##ContentSearch", "Search this folder", m_Search, sizeof(m_Search));
    }

    void ContentPanel::DrawGrid()
    {
        const Listing& listing = GetListing(m_Current);
        if (!listing.Exists)
        {
            ImGui::TextDisabled("'%s' cannot be listed.", m_Current.c_str());
            return;
        }

        const float cellHeight = ICON_SIZE + ImGui::GetTextLineHeightWithSpacing() + CELL_PADDING * 2.0f;
        const int   columns    = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / CELL_WIDTH));
        ImDrawList* drawList   = ImGui::GetWindowDrawList();

        std::string openFolder; // navigating while iterating the listing would change it underfoot
        int         column = 0;
        for (const FS::DirectoryEntry& entry : listing.Entries)
        {
            if (!ContainsIgnoringCase(entry.Name, m_Search))
                continue;
            const std::string path = JoinPath(m_Current, entry.Name);
            const ContentType type = GetContentType(path, entry.IsDirectory);

            if (column++ % columns != 0)
                ImGui::SameLine();
            ImGui::PushID(entry.Name.c_str());
            const ImVec2 cell = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##Entry", ImVec2(CELL_WIDTH - CELL_PADDING, cellHeight)))
                m_Selected = entry.Name;
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && entry.IsDirectory)
                openFolder = path;
            if (hovered)
                ImGui::SetTooltip("%s\n%s", entry.Name.c_str(), GetContentTypeName(type));

            if (entry.Name == m_Selected || hovered)
            {
                const ImU32 highlight = ImGui::GetColorU32(entry.Name == m_Selected ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered);
                drawList->AddRectFilled(cell, ImVec2(cell.x + CELL_WIDTH - CELL_PADDING, cell.y + cellHeight), highlight, 4.0f);
            }

            // The icon: a coloured tile with the type's glyph, then the name, cut to fit.
            const float  iconX   = cell.x + (CELL_WIDTH - CELL_PADDING - ICON_SIZE) * 0.5f;
            const ImVec2 iconMin = { iconX, cell.y + CELL_PADDING };
            const ImVec2 iconMax = { iconX + ICON_SIZE, iconMin.y + ICON_SIZE };
            drawList->AddRectFilled(iconMin, iconMax, IconColor(type), 6.0f);
            const char*  glyph     = GetContentTypeGlyph(type);
            const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
            drawList->AddText(ImVec2(iconMin.x + (ICON_SIZE - glyphSize.x) * 0.5f, iconMin.y + (ICON_SIZE - glyphSize.y) * 0.5f),
                              IM_COL32(20, 20, 20, 255), glyph);

            const std::string label     = FitToWidth(entry.Name, CELL_WIDTH - CELL_PADDING);
            const float       labelSize = ImGui::CalcTextSize(label.c_str()).x;
            drawList->AddText(ImVec2(cell.x + (CELL_WIDTH - CELL_PADDING - labelSize) * 0.5f, iconMax.y + CELL_PADDING * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
            ImGui::PopID();
        }
        if (column == 0)
            ImGui::TextDisabled(m_Search[0] != '\0' ? "Nothing here matches the search." : "This folder is empty.");

        if (!openFolder.empty())
            Navigate(openFolder);
    }
}
