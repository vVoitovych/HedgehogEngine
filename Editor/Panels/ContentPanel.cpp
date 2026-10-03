#include "ContentPanel.hpp"

#include "TextSearch.hpp"

#include "AssetDragDrop.hpp"

#include "Platform/ShellActions.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "imgui.h"

#include <algorithm>

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
    }

    ContentPanel::ContentPanel(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
        , m_Current(ROOT_FOLDER)
    {
    }

    std::optional<ContentOpenRequest> ContentPanel::Draw(const ContentIconIds& icons)
    {
        m_OpenRequest.reset();
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
        DrawGrid(icons);
        ImGui::EndChild();
        ImGui::EndGroup();
        return m_OpenRequest;
    }

    void ContentPanel::Activate(const std::string& path, ContentType type)
    {
        if (type == ContentType::Folder)
            m_PendingFolder = ContentOpenRequest{ path, type };
        else
            m_OpenRequest = ContentOpenRequest{ path, type };
    }

    void ContentPanel::DrawEntryMenu(const std::string& path, ContentType type)
    {
        if (!ImGui::BeginPopupContextItem("##EntryMenu"))
            return;
        const std::optional<std::filesystem::path> physical = m_FileSystem.ResolvePhysical(path);
        const std::string physicalText = physical ? std::filesystem::path(*physical).make_preferred().string() : std::string{};

        if (ImGui::MenuItem("Open"))
            Activate(path, type);
        if (ImGui::MenuItem("Show in Explorer", nullptr, false, physical.has_value()))
            (void)ShowInExplorer(*physical);
        ImGui::Separator();
        if (ImGui::MenuItem("Copy path (virtual)"))
            ImGui::SetClipboardText(path.c_str());
        if (ImGui::MenuItem("Copy path (physical)", nullptr, false, physical.has_value()))
            ImGui::SetClipboardText(physicalText.c_str());
        ImGui::EndPopup();
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

    void ContentPanel::DrawGrid(const ContentIconIds& icons)
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

        int column = 0;
        for (const FS::DirectoryEntry& entry : listing.Entries)
        {
            if (!ContainsIgnoringCase(entry.Name, m_Search))
                continue;
            const std::string path = JoinPath(m_Current, entry.Name);
            const ContentType type = GetContentType(path, entry.IsDirectory);
            void* const       icon = icons[static_cast<size_t>(type)];

            if (column++ % columns != 0)
                ImGui::SameLine();
            ImGui::PushID(entry.Name.c_str());
            const ImVec2 cell = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##Entry", ImVec2(CELL_WIDTH - CELL_PADDING, cellHeight)))
                m_Selected = entry.Name;
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                m_Selected = entry.Name;
            if (type != ContentType::Folder)
                DragAssetSource(path, type, entry.Name, icon);
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                Activate(path, type);
            if (hovered)
                ImGui::SetTooltip("%s\n%s", entry.Name.c_str(), GetContentTypeName(type));
            DrawEntryMenu(path, type);

            if (entry.Name == m_Selected || hovered)
            {
                const ImU32 highlight = ImGui::GetColorU32(entry.Name == m_Selected ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered);
                drawList->AddRectFilled(cell, ImVec2(cell.x + CELL_WIDTH - CELL_PADDING, cell.y + cellHeight), highlight, 4.0f);
            }

            // The type's icon, then the name, cut to fit.
            const float  iconX   = cell.x + (CELL_WIDTH - CELL_PADDING - ICON_SIZE) * 0.5f;
            const ImVec2 iconMin = { iconX, cell.y + CELL_PADDING };
            const ImVec2 iconMax = { iconX + ICON_SIZE, iconMin.y + ICON_SIZE };
            DrawAssetIcon(*drawList, iconMin, ICON_SIZE, type, icon);

            const std::string label     = FitToWidth(entry.Name, CELL_WIDTH - CELL_PADDING);
            const float       labelSize = ImGui::CalcTextSize(label.c_str()).x;
            drawList->AddText(ImVec2(cell.x + (CELL_WIDTH - CELL_PADDING - labelSize) * 0.5f, iconMax.y + CELL_PADDING * 0.5f),
                              ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
            ImGui::PopID();
        }
        if (column == 0)
            ImGui::TextDisabled(m_Search[0] != '\0' ? "Nothing here matches the search." : "This folder is empty.");

        // Navigating while iterating the listing would change it underfoot.
        if (m_PendingFolder)
        {
            Navigate(m_PendingFolder->VirtualPath);
            m_PendingFolder.reset();
        }
    }
}
