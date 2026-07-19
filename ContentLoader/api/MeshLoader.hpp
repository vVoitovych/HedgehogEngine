#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <optional>
#include <string>

namespace ContentLoader
{
    // Returns std::nullopt (after logging) when the path cannot be resolved,
    // the format is unsupported, or the file fails to parse.
    CONTENT_LOADER_API std::optional<LoadedMesh> LoadMesh(const std::string& fileName,
                                                           const FS::FileSystemManager& fileSystem);
}
