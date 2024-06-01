#include "CommonFunctions.hpp"

#include <Windows.h>
#include <stdexcept>
#include <filesystem>

namespace Hedgehog
{
	namespace ContentLoader
	{
		std::string GetCurrentDirectory()
		{
			char buffer[MAX_PATH];
			GetModuleFileNameA(NULL, buffer, MAX_PATH);
			std::string::size_type pos = std::string(buffer).find_last_of("\\/");

			return std::string(buffer).substr(0, pos);
		}

		std::string GetAssetsDirectory()
		{
			std::string path = GetCurrentDirectory();
			std::filesystem::path fsPath(path);
			std::string rootPath = fsPath.parent_path().parent_path().string();
			rootPath += "\\Assets\\";
			return rootPath;
		}

		std::string GetAssetRelativetlyPath(const std::string path)
		{
			std::string assetPath = GetAssetsDirectory();
			if (path.find(assetPath) == std::string::npos)
			{
				throw std::runtime_error("file isn\'t in asset forlder!");
			}

			return path.substr(assetPath.size());
		}

	}
}

