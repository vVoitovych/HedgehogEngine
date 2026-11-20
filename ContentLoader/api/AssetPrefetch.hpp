#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedData.hpp"
#include "LoadedEnvironment.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Assets decoded ahead of time, on any thread, for the next load of the same file to take. LoadMesh,
// TextureLoader::LoadTexture and BakeEnvironment look here first, so every consumer (the engine's
// meshes, the renderer's textures and environments) gets a prefetched asset with no change of its
// own. Entries are keyed by FS::MakeAssetKey, so any spelling of a path finds one, and taken once:
// the load after that reads the file again. Every call is safe from any thread.
// ponytail: one process-wide store inside the DLL; pass an explicit store if a second consumer ever
// needs its own.
namespace ContentLoader
{
    // A texture as TextureLoader decodes it: RGBA8, Width * Height * 4 bytes; Channels is the file's own.
    struct PrefetchedTexture
    {
        int                        Width    = 0;
        int                        Height   = 0;
        int                        Channels = 0;
        std::vector<unsigned char> Pixels;
    };

    // Each decodes path (as its loader would, logging what fails) and keeps the result; false when it
    // does not load. A path already kept is not decoded again (true).
    CONTENT_LOADER_API bool PrefetchMesh(const std::string& path, const FS::FileSystemManager& fileSystem);
    CONTENT_LOADER_API bool PrefetchTexture(const std::string& path, const FS::FileSystemManager& fileSystem);
    // Kept with desc: only a bake with the same face size and sample count takes it.
    CONTENT_LOADER_API bool PrefetchEnvironment(const std::string& path, const FS::FileSystemManager& fileSystem,
                                                const EnvironmentBakeDesc& desc = {});

    // Moves a kept entry out (nullopt when none, or for an environment kept with another desc).
    [[nodiscard]] CONTENT_LOADER_API std::optional<LoadedMesh>        TakeMesh(const std::string& path);
    [[nodiscard]] CONTENT_LOADER_API std::optional<PrefetchedTexture> TakeTexture(const std::string& path);
    [[nodiscard]] CONTENT_LOADER_API std::optional<BakedEnvironment>  TakeEnvironment(const std::string& path,
                                                                                      const EnvironmentBakeDesc& desc);

    // Drops every entry never taken.
    CONTENT_LOADER_API void ClearPrefetched();
    // The bytes the kept entries hold (vertex, index, skinning, pixel and texel data).
    [[nodiscard]] CONTENT_LOADER_API size_t GetPrefetchedBytes();
}
