#include "ContentPanel.hpp"

#include "TextSearch.hpp"

#include "AssetDragDrop.hpp"

#include "EditorTheme.hpp"
#include "Widgets/IconWidgets.hpp"

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

        constexpr float TREE_WIDTH    = 200.0f;
        constexpr float SLIDER_WIDTH  = 110.0f;
        constexpr float CELL_MARGIN   = 28.0f; // a cell is this much wider than its icon
        constexpr float CELL_PADDING  = 6.0f;

        // Spaces at the start of a tree row's label, where its folder icon is drawn.
        constexpr const char* TREE_ICON_PAD = "        ";

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

    ContentPanelRequest ContentPanel::Draw(const ContentPanelIcons& icons)
    {
        m_OpenRequest.reset();
        // A folder deleted while it was shown: fall back to the root.
        if (!GetListing(m_Current).Exists && m_Current != ROOT_FOLDER)
            Navigate(ROOT_FOLDER);

        ContentPanelRequest request;
        DrawHeader(icons, request);

        ImGui::BeginChild("ContentTree", ImVec2(TREE_WIDTH, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
        DrawFolderTree(ROOT_FOLDER, ROOT_LABEL, icons.Folder);
        m_RevealCurrent = false;
        ImGui::EndChild();
        m_TreeWidth = ImGui::GetItemRectSize().x;

        ImGui::SameLine();
        ImGui::BeginChild("ContentGrid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        DrawGrid(icons.Types);
        ImGui::EndChild();

        request.Open = m_OpenRequest;
        return request;
    }

    void ContentPanel::SetIconSize(float size)
    {
        m_IconSize = std::clamp(size, CONTENT_ICON_SIZE_MIN, CONTENT_ICON_SIZE_MAX);
    }

    // "+", the search box over the tree's column, the breadcrumb, and the icon-size slider at the
    // row's right end.
    void ContentPanel::DrawHeader(const ContentPanelIcons& icons, ContentPanelRequest& request)
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        if (IconButton("##ContentCreate", icons.Plus, ICON_SIZE_SMALL, style.Colors[ImGuiCol_Text]))
            ImGui::OpenPopup("##ContentCreateMenu");
        ImGui::SetItemTooltip("Create");
        if (ImGui::BeginPopup("##ContentCreateMenu"))
        {
            if (ImGui::MenuItem("Material..."))
                request.CreateMaterial = true;
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        const float treeWidth   = m_TreeWidth > 0.0f ? m_TreeWidth : TREE_WIDTH;
        const float searchWidth = std::max(treeWidth - ImGui::GetItemRectSize().x - style.ItemSpacing.x, 40.0f);
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##ContentSearch", "Search...", m_Search, sizeof(m_Search));

        ImGui::SameLine(0.0f, style.ItemSpacing.x * 2.0f);
        DrawBreadcrumb();

        ImGui::SameLine();
        const float sliderX = ImGui::GetWindowContentRegionMax().x - SLIDER_WIDTH;
        if (sliderX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(sliderX);
        ImGui::SetNextItemWidth(SLIDER_WIDTH);
        ImGui::SliderFloat("##ContentIconSize", &m_IconSize, CONTENT_ICON_SIZE_MIN, CONTENT_ICON_SIZE_MAX, "",
                           ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetItemTooltip("Icon size");
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

    void ContentPanel::DrawFolderTree(const std::string& folder, const std::string& label, void* folderIcon)
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

        const float labelX = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
        const bool  open   = ImGui::TreeNodeEx(folder.c_str(), flags, "%s%s", TREE_ICON_PAD, label.c_str());
        const float rowY   = ImGui::GetItemRectMin().y + (ImGui::GetItemRectSize().y - ICON_SIZE_SMALL) * 0.5f;
        DrawIcon(*ImGui::GetWindowDrawList(), folderIcon, ImVec2(labelX, rowY), ICON_SIZE_SMALL,
                 ImGui::GetColorU32(ImGuiCol_Text));
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            Navigate(folder);
        if (!open)
            return;
        for (const std::string& name : subfolders)
            DrawFolderTree(JoinPath(folder, name), name, folderIcon);
        ImGui::TreePop();
    }

    void ContentPanel::DrawBreadcrumb()
    {
        // "Assets > Models > Default", each part a flat button that goes there.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        std::string target = ROOT_FOLDER;
        if (ImGui::Button(ROOT_LABEL))
            Navigate(ROOT_FOLDER);

        const std::string relative = m_Current.substr(std::string_view(ROOT_FOLDER).size());
        size_t start = 0;
        int    part  = 0;
        while (start < relative.size())
        {
            const size_t      end  = std::min(relative.find('/', start), relative.size());
            const std::string name = relative.substr(start, end - start);
            target = JoinPath(target, name);
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(">");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::PushID(part++);
            if (ImGui::Button(name.c_str()))
                Navigate(target);
            ImGui::PopID();
            start = end + 1;
        }
        ImGui::PopStyleColor();
    }

    void ContentPanel::DrawGrid(const ContentIconIds& icons)
    {
        const Listing& listing = GetListing(m_Current);
        if (!listing.Exists)
        {
            ImGui::TextDisabled("'%s' cannot be listed.", m_Current.c_str());
            return;
        }

        const float iconSize   = m_IconSize;
        const float cellWidth  = iconSize + CELL_MARGIN;
        const float lineHeight = ImGui::GetTextLineHeight();
        const float cellHeight = iconSize + 2.0f * lineHeight + CELL_PADDING * 3.0f;
        const int   columns    = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth));
        ImDrawList* drawList   = ImGui::GetWindowDrawList();
        const ImU32 textColor  = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 typeColor  = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const ImU32 frameColor = ImGui::ColorConvertFloat4ToU32(Theme::Resolve(Theme::ACCENT));

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
            const ImVec2 cellMax = { cell.x + cellWidth - CELL_PADDING, cell.y + cellHeight };
            if (ImGui::InvisibleButton("##Entry", ImVec2(cellWidth - CELL_PADDING, cellHeight)))
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

            const bool selected = entry.Name == m_Selected;
            if (selected || hovered)
            {
                const ImU32 highlight = ImGui::GetColorU32(selected ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered);
                drawList->AddRectFilled(cell, cellMax, highlight, 4.0f);
                if (selected)
                    drawList->AddRect(cell, cellMax, frameColor, 4.0f, 0, 1.0f);
            }

            // The type's icon, then the name cut to fit, then the type, muted.
            const float  innerWidth = cellWidth - CELL_PADDING;
            const float  iconX      = cell.x + (innerWidth - iconSize) * 0.5f;
            const ImVec2 iconMin    = { iconX, cell.y + CELL_PADDING };
            DrawAssetIcon(*drawList, iconMin, iconSize, type, icon);

            const float       nameY    = iconMin.y + iconSize + CELL_PADDING;
            const std::string label    = FitToWidth(entry.Name, innerWidth - 4.0f);
            const float       nameSize = ImGui::CalcTextSize(label.c_str()).x;
            drawList->AddText(ImVec2(cell.x + (innerWidth - nameSize) * 0.5f, nameY), textColor, label.c_str());

            const char* typeName = GetContentTypeName(type);
            const float typeSize = ImGui::CalcTextSize(typeName).x;
            drawList->AddText(ImVec2(cell.x + (innerWidth - typeSize) * 0.5f, nameY + lineHeight), typeColor, typeName);
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
