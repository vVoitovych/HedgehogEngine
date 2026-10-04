#pragma once

#include "ContentIcons.hpp"
#include "ContentTypes.hpp"

#include "FileSystem/api/FileSystem.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace Editor
{
    // A file the user asked to open (double-click, or Open in its context menu); what opening
    // means depends on its type, and is the editor's to decide (EditorGui::OpenContentItem).
    struct ContentOpenRequest
    {
        std::string VirtualPath;
        ContentType Type = ContentType::Other;
    };

    // The grid's icon size in pixels: the slider's range and a fresh layout's size.
    inline constexpr float CONTENT_ICON_SIZE_MIN     = 40.0f;
    inline constexpr float CONTENT_ICON_SIZE_MAX     = 128.0f;
    inline constexpr float CONTENT_ICON_SIZE_DEFAULT = 64.0f;

    // The pictures the panel draws: each type's (ContentIcons) and the line icons of its header.
    struct ContentPanelIcons
    {
        ContentIconIds Types  = {};
        void*          Folder = nullptr; // before each folder in the tree
        void*          Plus   = nullptr; // the create menu's button
    };

    // What the user asked for this frame: a file to open, or a new material.
    struct ContentPanelRequest
    {
        std::optional<ContentOpenRequest> Open;
        bool                              CreateMaterial = false;
    };

    // The Content panel, shown as Project: browses Assets/ through the virtual file system
    // ("assets://"). A header row of a "+" create menu, a search box over the folder tree that
    // filters the current folder by name, a breadcrumb and an icon-size slider; below it the folder
    // tree on the left and a grid of the folder's entries, each an icon for its type
    // (ContentTypes.hpp) with its name and type below. Listings are cached and re-read at most once
    // a second, never every frame. Double-clicking a folder opens it; a file is handed back from
    // Draw() as a request. Each entry's context menu has Open, Show in Explorer and Copy path
    // (virtual or physical).
    class ContentPanel
    {
    public:
        explicit ContentPanel(const FS::FileSystemManager& fileSystem);

        ContentPanel(const ContentPanel&)            = delete;
        ContentPanel& operator=(const ContentPanel&) = delete;
        ContentPanel(ContentPanel&&)                 = delete;
        ContentPanel& operator=(ContentPanel&&)      = delete;

        [[nodiscard]] ContentPanelRequest Draw(const ContentPanelIcons& icons);

        // Shows the file's folder in the grid with the file selected (Select in the prefab bar).
        void Reveal(const std::string& virtualPath);

        // The grid's icon size, clamped to [CONTENT_ICON_SIZE_MIN, CONTENT_ICON_SIZE_MAX].
        [[nodiscard]] float GetIconSize() const { return m_IconSize; }
        void                SetIconSize(float size);

    private:
        struct Listing
        {
            std::vector<FS::DirectoryEntry>       Entries;
            bool                                  Exists = false;
            std::chrono::steady_clock::time_point Scanned;
        };

        // The folder's cached listing, read again when it is older than the refresh interval.
        const Listing& GetListing(const std::string& folder);
        void Navigate(const std::string& folder);

        void DrawHeader(const ContentPanelIcons& icons, ContentPanelRequest& request);
        void DrawFolderTree(const std::string& folder, const std::string& label, void* folderIcon);
        void DrawBreadcrumb();
        void DrawGrid(const ContentIconIds& icons);
        void DrawEntryMenu(const std::string& path, ContentType type);
        // A folder is navigated into; a file becomes this frame's open request.
        void Activate(const std::string& path, ContentType type);

        const FS::FileSystemManager&             m_FileSystem;
        std::unordered_map<std::string, Listing> m_Listings; // by virtual folder path
        std::string                              m_Current;  // the folder the grid shows
        std::string                              m_Selected; // the selected entry's name in it
        char                                     m_Search[128] = {};
        bool                                     m_RevealCurrent = true; // open the tree down to m_Current
        float                                    m_IconSize  = CONTENT_ICON_SIZE_DEFAULT;
        float                                    m_TreeWidth = 0.0f; // as the user last resized it
        std::optional<ContentOpenRequest>        m_OpenRequest;   // this frame's
        std::optional<ContentOpenRequest>        m_PendingFolder; // opened after the grid's loop
    };
}
