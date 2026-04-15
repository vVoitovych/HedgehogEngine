#pragma once

#include "api/LoadedData.hpp"

#include <string>

namespace ContentLoader
{
    LoadedMesh LoadGltfMesh(const std::string& path);
}
