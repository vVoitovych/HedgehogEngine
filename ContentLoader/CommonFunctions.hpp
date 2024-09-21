#pragma once

#include <string>

namespace ContentLoader
{
	std::string GetRootDirectory();

	std::string GetAssetsDirectory();

	std::string GetAssetRelativetlyPath(const std::string path);

	std::string GetShadersDirectory();

}


