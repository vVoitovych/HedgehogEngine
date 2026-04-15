#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedData.hpp"

#include <string>

namespace ContentLoader
{
    CONTENT_LOADER_API LoadedMesh LoadMesh(const std::string& fileName);
}
