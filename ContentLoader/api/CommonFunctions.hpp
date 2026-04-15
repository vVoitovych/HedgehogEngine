#pragma once

#include "ContentLoaderApi.hpp"

#include <string>

namespace ContentLoader
{
    CONTENT_LOADER_API std::string GetRootDirectory();

    CONTENT_LOADER_API std::string GetAssetsDirectory();

    CONTENT_LOADER_API std::string GetAssetRelativetlyPath(const std::string path);

    CONTENT_LOADER_API std::string GetShadersDirectory();

    CONTENT_LOADER_API std::string ReadFile(const std::string& filepath);
}
