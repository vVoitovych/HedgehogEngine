#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace HedgehogEngine
{
    class MeshSystem;
    class Mesh;
    class TerrainContainer;
    struct TerrainMesh;

    class MeshContainer
    {
    public:
        HEDGEHOG_ENGINE_API MeshContainer();
        HEDGEHOG_ENGINE_API ~MeshContainer();

        MeshContainer(const MeshContainer&)            = delete;
        MeshContainer(MeshContainer&&)                 = delete;
        MeshContainer& operator=(const MeshContainer&) = delete;
        MeshContainer& operator=(MeshContainer&&)      = delete;

        // Loads the meshes MeshSystem listed since the last call: a file each, or, for a
        // MakeTerrainMeshPath path, the grid of that terrain in terrains as it is now (empty without
        // it).
        HEDGEHOG_ENGINE_API void Update(const MeshSystem& meshSystem, const FS::FileSystemManager& fileSystem,
                                        TerrainContainer* terrains = nullptr);

        HEDGEHOG_ENGINE_API size_t      GetMeshCount() const;
        HEDGEHOG_ENGINE_API const Mesh& GetMesh(size_t index) const;
        // The index of the mesh listed under path, or std::nullopt.
        HEDGEHOG_ENGINE_API std::optional<size_t> FindMesh(const std::string& path) const;

        // Replaces a mesh's geometry in place and bumps its version; false, changing nothing, when
        // the vertex or index count differs (the meshes are packed back to back).
        HEDGEHOG_ENGINE_API bool     ReplaceGeometry(size_t index, TerrainMesh geometry);
        HEDGEHOG_ENGINE_API uint32_t GetMeshVersion(size_t index) const;

    private:
        std::vector<std::string> m_FilePaths;
        std::vector<Mesh>        m_Meshes;
        std::vector<uint32_t>    m_Versions;
        uint32_t                 m_TotalVertexCount = 0;
        uint32_t                 m_TotalIndexCount  = 0;
    };
}
