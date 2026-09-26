#pragma once

#include "GraphAsset.hpp"
#include "PassBuilderRegistry.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    // RENDERING.md section 6, "Failure behaviour". Owns the graph assets views instantiate from,
    // by name (a CameraComponent's GraphName), and hot-reloads them.
    //
    // Each name keeps its last known-good asset: one that parsed and passed
    // GraphInstantiator::Validate against the registry. A reload that fails either step logs a
    // named error, records it for GetLastError(), and leaves the known-good asset in place, so a
    // typo never takes a view down. Because graphs are rebuilt from their asset every frame and
    // passes hold no state, replacing the asset is all a hot reload needs: the next frame builds
    // the new graph.
    //
    // Works on physical paths and does no FileSystem mount resolution; the caller resolves
    // "engine://" paths first. FrameRenderer registers the engine's graph directory with it.
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

        // The last known-good asset, or nullptr if name has never loaded successfully.
        [[nodiscard]] const GraphAsset* Find(std::string_view name) const;

        // Why the most recent load of name failed; empty if it succeeded or name is unknown.
        [[nodiscard]] std::string_view GetLastError(std::string_view name) const;

        // Every registered name in sorted order, whether or not it has a known-good asset.
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
        std::vector<std::string>               m_Names; // sorted keys of m_Entries

        std::filesystem::path           m_Directory;
        std::filesystem::file_time_type m_DirectoryWriteTime{};
    };
}
