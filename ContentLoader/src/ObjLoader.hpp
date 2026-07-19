#pragma once

#include "api/LoadedData.hpp"

#include <optional>
#include <string>

namespace ContentLoader
{
    std::optional<LoadedMesh> LoadObj(const std::string& path);
}
