#pragma once

#include "GraphAsset.hpp"
#include "PassBuilderRegistry.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    // RENDERING.md section 6, "Failure behaviour". Owns the graph assets views instantiate from,
    // by graph reference (a CameraComponent's GraphName, GraphReference.hpp), and hot-reloads them:
    // the engine's graphs by name, and .graph files anywhere by path, loaded on first use.
    //
    // Each name keeps its last known-good asset: one that parsed and passed
    // GraphInstantiator::Validate against the registry. A reload that fails either step logs a
    // named error, records it for GetLastError(), and leaves the known-good asset in place, so a
    // typo never takes a view down. Because graphs are rebuilt from their asset every frame and
    // passes hold no state, replacing the asset is all a hot reload needs: the next frame builds
    // the new graph.
    //
    // Works on physical paths and does no FileSystem mount resolution itself: FindOrLoad takes the
    // resolver. FrameRenderer registers the engine's graph directory with it.
    class GraphAssetLibrary
    {
    public:
        explicit GraphAssetLibrary(const PassBuilderRegistry& registry) : m_Registry(registry) {}

        // Loads file and watches it under name. Returns false if the first load fails; the name
        // stays registered with no known-good asset, and a fixed edit is picked up by Poll().
        [[nodiscard]] bool Register(const std::string& name, const std::filesystem::path& file);

        // Registers every "*.graph" file in directory under its file stem ("game.graph" is "game"),
        // and watches the directory: Poll() registers files added to it later. Returns false if the
        // directory does not exist. A file that fails its first load is registered all the same,
        // like Register().
        bool RegisterDirectory(const std::filesystem::path& directory);

        // Registers files added to the watched directory, then reloads every watched file whose
        // write time changed since it was last read. Returns the names whose known-good asset was
        // added or replaced; call once per frame, before building graphs. The directory is only
        // listed when its own write time changes, so an unchanged directory costs one stat.
        std::vector<std::string> Poll();

        // Resolves a virtual path ("assets://...") to a physical file, or nothing.
        using VirtualPathResolver = std::function<std::optional<std::filesystem::path>(std::string_view)>;

        // The last known-good asset a graph reference names, registering a file reference on first
        // use: it is loaded, watched by Poll() like any other, and keyed by its normalized reference.
        // A file that does not exist is not registered; GetLastError says so, and each later call
        // looks for it again, so a file created afterwards is picked up. nullptr for an unknown name.
        const GraphAsset* FindOrLoad(std::string_view reference, const VirtualPathResolver& resolveVirtual);

        // The last known-good asset, or nullptr if reference has never loaded successfully. Unlike
        // FindOrLoad, never registers anything.
        [[nodiscard]] const GraphAsset* Find(std::string_view reference) const;

        // Why the most recent load of reference failed, or that its file does not exist; empty if
        // it loaded or is unknown.
        [[nodiscard]] std::string_view GetLastError(std::string_view reference) const;

        // The file reference was registered from; empty if it is unknown.
        [[nodiscard]] std::filesystem::path GetFile(std::string_view reference) const;

        // Every registered reference, whether or not it has a known-good asset: names in sorted
        // order, then file references in sorted order.
        [[nodiscard]] const std::vector<std::string>& GetNames() const { return m_Names; }

    private:
        struct Entry
        {
            std::filesystem::path           File;
            std::filesystem::file_time_type LastWriteTime{};
            std::optional<GraphAsset>       KnownGood;
            std::string                     LastError;
        };

        bool Load(const std::string& name, Entry& entry) const;
        // Registers the watched directory's graph files that are not registered yet. Returns the
        // names that loaded.
        std::vector<std::string> RegisterNewDirectoryFiles();

        const PassBuilderRegistry&             m_Registry;
        std::unordered_map<std::string, Entry> m_Entries;
        std::vector<std::string>               m_Names; // keys of m_Entries, names before files
        // File references FindOrLoad could not find, with the error GetLastError reports.
        std::unordered_map<std::string, std::string> m_MissingFiles;

        std::filesystem::path           m_Directory;
        std::filesystem::file_time_type m_DirectoryWriteTime{};
    };
}
