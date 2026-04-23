#include "MeshContainer.hpp"
#include "Mesh.hpp"

#include "Scene/Scene.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace Context
{
    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::Update(const Scene::Scene& scene)
    {
        const auto& meshPaths = scene.GetMeshes();
        if (meshPaths.size() <= m_Meshes.size())
            return;

        for (size_t i = m_Meshes.size(); i < meshPaths.size(); ++i)
        {
            const auto& path = meshPaths[i];
            if (std::find(m_FilePaths.begin(), m_FilePaths.end(), path) != m_FilePaths.end())
            {
                LOGWARNING("Mesh ", path, " already added");
                continue;
            }
            m_FilePaths.push_back(path);

            Mesh mesh;
            mesh.LoadData(path);
            mesh.SetVertexOffset(m_TotalVertexCount);
            mesh.SetFirstIndex(m_TotalIndexCount);
            m_TotalVertexCount += static_cast<uint32_t>(mesh.GetPositions().size());
            m_TotalIndexCount  += static_cast<uint32_t>(mesh.GetIndices().size());

            LOGINFO("Loaded mesh data: ", path);
            m_Meshes.push_back(std::move(mesh));
        }
    }

    size_t MeshContainer::GetMeshCount() const
    {
        return m_Meshes.size();
    }

    const Mesh& MeshContainer::GetMesh(size_t index) const
    {
        return m_Meshes[index];
    }
}
