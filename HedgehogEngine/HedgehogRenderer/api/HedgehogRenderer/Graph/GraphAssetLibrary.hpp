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
    // "engine://" paths first. Nothing in the frame loop uses it yet (HE-83 does).
    class GraphAssetLibrary
    {
    public:
        explicit GraphAssetLibrary(const PassBuilderRegistry& registry) : m_Registry(registry) {}

        // Loads file and watches it under name. Returns false if the first load fails; the name
        // stays registered with no known-good asset, and a fixed edit is picked up by Poll().
        [[nodiscard]] bool Register(const std::string& name, const std::filesystem::path& file);

        // Reloads every watched file whose write time changed since it was last read. Returns the
        // names whose known-good asset was replaced; call once per frame, before building graphs.
        std::vector<std::string> Poll();

        // The last known-good asset, or nullptr if name has never loaded successfully.
        [[nodiscard]] const GraphAsset* Find(std::string_view name) const;

        // Why the most recent load of name failed; empty if it succeeded or name is unknown.
        [[nodiscard]] std::string_view GetLastError(std::string_view name) const;

    private:
        struct Entry
        {
            std::filesystem::path           File;
            std::filesystem::file_time_type LastWriteTime{};
            std::optional<GraphAsset>       KnownGood;
            std::string                     LastError;
        };

        bool Load(const std::string& name, Entry& entry) const;

        const PassBuilderRegistry&             m_Registry;
        std::unordered_map<std::string, Entry> m_Entries;
    };
}
