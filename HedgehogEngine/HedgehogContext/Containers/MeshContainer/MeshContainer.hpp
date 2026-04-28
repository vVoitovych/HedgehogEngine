#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include <vector>
#include <string>
#include <cstddef>

namespace Scene
{
    class MeshSystem;
}

namespace Context
{
    class Mesh;

    class MeshContainer
    {
    public:
        HEDGEHOG_CONTEXT_API MeshContainer();
        HEDGEHOG_CONTEXT_API ~MeshContainer();

        MeshContainer(const MeshContainer&)            = delete;
        MeshContainer(MeshContainer&&)                 = delete;
        MeshContainer& operator=(const MeshContainer&) = delete;
        MeshContainer& operator=(MeshContainer&&)      = delete;

        HEDGEHOG_CONTEXT_API void Update(const Scene::MeshSystem& meshSystem);

        HEDGEHOG_CONTEXT_API size_t      GetMeshCount() const;
        HEDGEHOG_CONTEXT_API const Mesh& GetMesh(size_t index) const;

    private:
        std::vector<std::string> m_FilePaths;
        std::vector<Mesh>        m_Meshes;
        uint32_t                 m_TotalVertexCount = 0;
        uint32_t                 m_TotalIndexCount  = 0;
    };
}
