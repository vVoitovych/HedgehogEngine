#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace HedgehogEngine
{
    // A terrain's mesh is listed with the meshes as terrain:<resolution>:<its file's key>, so the
    // MeshContainer builds it from the TerrainContainer instead of reading a file, and a terrain
    // given another resolution is a mesh of its own (meshes are packed back to back, so one cannot
    // change its vertex count in place).
    inline constexpr std::string_view TERRAIN_MESH_PREFIX = "terrain:";

    [[nodiscard]] inline std::string MakeTerrainMeshPath(const std::string& terrainKey, uint32_t resolution)
    {
        return std::string(TERRAIN_MESH_PREFIX) + std::to_string(resolution) + ":" + terrainKey;
    }

    // The terrain's key a MakeTerrainMeshPath path names; empty for any other path.
    [[nodiscard]] inline std::string_view GetTerrainMeshKey(std::string_view meshPath)
    {
        if (!meshPath.starts_with(TERRAIN_MESH_PREFIX))
            return {};
        const std::string_view rest  = meshPath.substr(TERRAIN_MESH_PREFIX.size());
        const size_t           colon = rest.find(':');
        return colon == std::string_view::npos ? std::string_view{} : rest.substr(colon + 1);
    }

    // What changed in a terrain since the renderer and physics last took it, a bit each.
    enum class TerrainDirty : uint8_t
    {
        None    = 0,
        Heights = 1 << 0,
        Splat   = 1 << 1,
        Layers  = 1 << 2,
        All     = Heights | Splat | Layers,
    };

    // The loaded terrains, one per file however its path is spelt (FS::MakeAssetKey). The editor
    // edits them in place and saves them; terrains are never unloaded, so indices stay valid for the
    // container's lifetime. A newly loaded terrain is dirty in every part.
    class TerrainContainer
    {
    public:
        HEDGEHOG_ENGINE_API explicit TerrainContainer(const FS::FileSystemManager& fileSystem);

        // The index of path (under assets:// unless it names a mount), loading it on first use.
        // std::nullopt for an empty path, or a file that does not read or parse, which is logged
        // once and not retried.
        HEDGEHOG_ENGINE_API std::optional<size_t> FindOrLoad(const std::string& path);

        HEDGEHOG_ENGINE_API size_t             GetTerrainCount() const;
        // The file's key (FS::MakeAssetKey); FS::ToAssetVirtualPath of it names the file.
        HEDGEHOG_ENGINE_API const std::string& GetPath(size_t index) const;
        HEDGEHOG_ENGINE_API const TerrainData& GetTerrain(size_t index) const;
        // For editing: mark what changed with MarkDirty.
        HEDGEHOG_ENGINE_API TerrainData&       GetTerrain(size_t index);

        HEDGEHOG_ENGINE_API void         MarkDirty(size_t index, TerrainDirty parts);
        HEDGEHOG_ENGINE_API TerrainDirty GetDirty(size_t index) const;
        HEDGEHOG_ENGINE_API void         ClearDirty(size_t index, TerrainDirty parts);

        // Writes the terrain to its file; false, logged, when it cannot be written.
        HEDGEHOG_ENGINE_API bool Save(size_t index) const;

    private:
        struct Entry
        {
            std::string                Path;   // the key
            std::optional<TerrainData> Data;   // empty when it failed to load
            uint8_t                    Dirty = 0;
        };

        const FS::FileSystemManager& m_FileSystem;
        std::vector<Entry>           m_Entries; // every key tried, failures included
        std::vector<size_t>          m_Loaded;  // terrain index -> entry index
    };

    [[nodiscard]] constexpr TerrainDirty operator|(TerrainDirty a, TerrainDirty b)
    {
        return static_cast<TerrainDirty>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr bool HasAny(TerrainDirty value, TerrainDirty parts)
    {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(parts)) != 0;
    }
}
