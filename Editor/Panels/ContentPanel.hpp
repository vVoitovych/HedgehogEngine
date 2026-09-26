#pragma once

#include "FileSystem/api/FileSystem.hpp"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace Editor
{
    // The Content panel: browses Assets/ through the virtual file system ("assets://"). A folder
    // tree on the left; on the right a breadcrumb, a search box that filters the current folder by
    // name, and a grid of the folder's entries, each an icon for its type (ContentTypes.hpp) with
    // its name below. Listings are cached and re-read at most once a second, never every frame.
    class ContentPanel
    {
    public:
        explicit ContentPanel(const FS::FileSystemManager& fileSystem);

        ContentPanel(const ContentPanel&)            = delete;
        ContentPanel& operator=(const ContentPanel&) = delete;
        ContentPanel(ContentPanel&&)                 = delete;
        ContentPanel& operator=(ContentPanel&&)      = delete;

        void Draw();

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

        void DrawFolderTree(const std::string& folder, const std::string& label);
        void DrawBreadcrumb();
        void DrawGrid();

        const FS::FileSystemManager&             m_FileSystem;
        std::unordered_map<std::string, Listing> m_Listings; // by virtual folder path
        std::string                              m_Current;  // the folder the grid shows
        std::string                              m_Selected; // the selected entry's name in it
        char                                     m_Search[128] = {};
        bool                                     m_RevealCurrent = true; // open the tree down to m_Current
    };
}
