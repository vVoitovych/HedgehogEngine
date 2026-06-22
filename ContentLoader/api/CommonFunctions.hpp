#pragma once

#include "ContentLoaderApi.hpp"

#include "FileSystem/api/FileSystem.hpp"

#include <memory>
#include <string>

namespace ContentLoader
{
    CONTENT_LOADER_API std::string GetRootDirectory();

    CONTENT_LOADER_API std::string GetAssetsDirectory();

    CONTENT_LOADER_API std::string GetAssetRelativelyPath(const std::string path);

    CONTENT_LOADER_API std::string GetShadersDirectory();

    // Creates a FileSystem with engine://, assets://, and shaders:// mount points
    // registered from the exe-relative physical directories.
    CONTENT_LOADER_API std::unique_ptr<FS::FileSystem> CreateEngineFileSystem();
}
