#include "api/MeshLoader.hpp"

#include "ObjLoader.hpp"
#include "GltfMeshLoader.hpp"

#include "Logger/api/Logger.hpp"

#include <filesystem>

namespace ContentLoader
{
    std::optional<LoadedMesh> LoadMesh(const std::string& fileName,
                                        const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = "assets://" + fileName;
        const auto physPath = fileSystem.ResolvePhysical(virtualPath);
        if (!physPath)
        {
            LOGERROR("Cannot resolve mesh path: ", virtualPath);
            return std::nullopt;
        }

        const std::string path      = physPath->string();
        const std::string extension = std::filesystem::path(path).extension().string();

        if (extension == ".obj")
            return LoadObj(path);
        if (extension == ".gltf")
            return LoadGltfMesh(path);

        LOGERROR("Unsupported mesh file format: ", extension);
        return std::nullopt;
    }
}
