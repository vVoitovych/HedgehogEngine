#pragma once

#include <string>

namespace ContentLoader
{
	std::string GetCurrentDirectory();

	std::string GetAssetsDirectory();

	std::string GetAssetRelativetlyPath(const std::string path);
}


