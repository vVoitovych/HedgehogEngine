#include "api/MeshLoader.hpp"

#include "api/AssetPrefetch.hpp"
#include "api/MeshLods.hpp"

#include "ObjLoader.hpp"
#include "GltfMeshLoader.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <filesystem>

namespace ContentLoader
{
    std::optional<LoadedMesh> LoadMesh(const std::string& fileName,
                                        const FS::FileSystemManager& fileSystem)
    {
        if (std::optional<LoadedMesh> prefetched = TakeMesh(fileName))
            return prefetched;
        const std::string virtualPath = FS::ToAssetVirtualPath(fileName);
        const auto physPath = fileSystem.ResolvePhysical(virtualPath);
        if (!physPath)
        {
            LOGERROR("Cannot resolve mesh path: ", virtualPath);
            return std::nullopt;
        }

        const std::string path      = physPath->string();
        const std::string extension = std::filesystem::path(path).extension().string();

        std::optional<LoadedMesh> mesh;
        if (extension == ".obj")
            mesh = LoadObj(path);
        else if (extension == ".gltf" || extension == ".glb")
            mesh = LoadGltfMesh(path);
        else
        {
            LOGERROR("Unsupported mesh file format: ", extension);
            return std::nullopt;
        }
        if (mesh)
        {
            std::vector<HM::Vector3> positions;
            positions.reserve(mesh->vertices.size());
            for (const LoadedVertexData& vertex : mesh->vertices)
                positions.push_back(vertex.position);
            mesh->LodIndices = GenerateMeshLods(positions, mesh->indices);
        }
        return mesh;
    }
}
