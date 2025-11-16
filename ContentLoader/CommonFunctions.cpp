#include "CommonFunctions.hpp"

#include <Windows.h>
#include <stdexcept>
#include <filesystem>
#include <fstream>

namespace ContentLoader
{
	std::string GetRootDirectory()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::string::size_type pos = std::string(buffer).find_last_of("\\/");

		std::string programPath =  std::string(buffer).substr(0, pos);
		std::filesystem::path fsPath(programPath);
		std::string rootPath = fsPath.parent_path().parent_path().parent_path().parent_path().string();
		return rootPath;
	}

	std::string GetAssetsDirectory()
	{
		std::string path = GetRootDirectory();
		path += "\\Assets\\";
		return path;
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

	std::string GetShadersDirectory()
	{
		std::string path = GetRootDirectory();
		path += "\\Shaders\\Shaders\\";
		return path;
	}

	std::string ReadFile(const std::string& filepath)
	{
		std::string path = GetRootDirectory();
		path += filepath;
		std::ifstream file(path, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file: " + path);
		}

		size_t fileSize = (size_t)file.tellg();
		std::string buffer(fileSize, '\0');
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		return buffer;
	}

}


