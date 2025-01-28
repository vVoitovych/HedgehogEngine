#pragma once

#include "LoadedMesh.hpp"

#include <string>

namespace ContentLoader
{
	LoadedMesh LoadGltfMesh(const std::string& path);

}

