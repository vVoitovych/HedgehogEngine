#include "api/CommonFunctions.hpp"

#include "FileSystem/api/FileSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <Windows.h>
#include <cassert>
#include <stdexcept>
#include <filesystem>

namespace ContentLoader
{
    std::string GetRootDirectory()
    {
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string::size_type pos = std::string(buffer).find_last_of("\\/");

        std::string programPath = std::string(buffer).substr(0, pos);
        std::filesystem::path fsPath(programPath);
        std::string rootPath = fsPath.parent_path().parent_path().parent_path().parent_path().string();
        assert(std::filesystem::exists(rootPath) && "Engine root not found — binary must be 4 levels deep (Binaries/Windows-x64/<Config>/<Project>/)");
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

    std::unique_ptr<FS::FileSystem> CreateEngineFileSystem()
    {
        auto fileSystem = std::make_unique<FS::FileSystem>();
        const bool okEngine  = fileSystem->RegisterPath("engine://",  GetRootDirectory());
        const bool okAssets  = fileSystem->RegisterPath("assets://",  GetAssetsDirectory());
        const bool okShaders = fileSystem->RegisterPath("shaders://", GetShadersDirectory());
        if (!okEngine || !okAssets || !okShaders)
            LOGERROR("CreateEngineFileSystem: one or more mount points failed to register — file I/O will be broken.");
        assert(okEngine  && "engine:// mount failed");
        assert(okAssets  && "assets:// mount failed");
        assert(okShaders && "shaders:// mount failed");
        return fileSystem;
    }
}
