#pragma once

#include <vector>
#include <string>
#include <cstddef>

namespace Scene
{
    class Scene;
}

namespace Context
{
    class Mesh;

    class MeshContainer
    {
    public:
        MeshContainer();
        ~MeshContainer();

        MeshContainer(const MeshContainer&)            = delete;
        MeshContainer(MeshContainer&&)                 = delete;
        MeshContainer& operator=(const MeshContainer&) = delete;
        MeshContainer& operator=(MeshContainer&&)      = delete;

        void Update(const Scene::Scene& scene);

        size_t      GetMeshCount() const;
        const Mesh& GetMesh(size_t index) const;

    private:
        std::vector<std::string> m_FilePaths;
        std::vector<Mesh>        m_Meshes;
        uint32_t                 m_TotalVertexCount = 0;
        uint32_t                 m_TotalIndexCount  = 0;
    };
}
