#pragma once

#include "api/LoadedData.hpp"

#include <optional>
#include <string>

namespace ContentLoader
{
    std::optional<LoadedMesh> LoadGltfMesh(const std::string& path);
}
