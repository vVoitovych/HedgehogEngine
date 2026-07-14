#include "api/MeshLoader.hpp"

#include "ObjLoader.hpp"
#include "GltfMeshLoader.hpp"

#include <filesystem>
#include <stdexcept>

namespace ContentLoader
{
    LoadedMesh LoadMesh(const std::string& fileName,
                         const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = "assets://" + fileName;
        const auto physPath = fileSystem.ResolvePhysical(virtualPath);
        if (!physPath)
            throw std::runtime_error("Cannot resolve mesh path: " + virtualPath);

        const std::string path      = physPath->string();
        const std::string extension = std::filesystem::path(path).extension().string();

        if (extension == ".obj")
            return LoadObj(path);
        else if (extension == ".gltf")
            return LoadGltfMesh(path);
        else
            throw std::runtime_error("unsupported file format: " + extension);
    }
}
