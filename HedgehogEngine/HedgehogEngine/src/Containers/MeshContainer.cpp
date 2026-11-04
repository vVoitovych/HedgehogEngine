#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"

#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    MeshContainer::MeshContainer()
    {
    }

    MeshContainer::~MeshContainer()
    {
    }

    void MeshContainer::Update(const MeshSystem& meshSystem, const FS::FileSystemManager& fileSystem,
                               TerrainContainer* terrains)
    {
        const auto& meshPaths = meshSystem.GetMeshes();
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

            Mesh mesh;
            // On failure keep the empty mesh anyway: entries must stay index-aligned
            // with MeshSystem::GetMeshes(), and an empty mesh renders nothing.
            if (path.starts_with(TERRAIN_MESH_PREFIX))
            {
                const std::string_view      key     = GetTerrainMeshKey(path);
                const std::optional<size_t> terrain =
                    terrains && !key.empty() ? terrains->FindOrLoad(std::string(key)) : std::nullopt;
                if (terrain)
                {
                    TerrainMesh grid = BuildTerrainMesh(terrains->GetTerrain(*terrain));
                    mesh.SetGeometry(std::move(grid.Positions), std::move(grid.TexCoords), std::move(grid.Normals),
                                     std::move(grid.Tangents), std::move(grid.Indices));
                }
            }
            else
            {
                mesh.LoadData(path, fileSystem);
            }
            mesh.SetVertexOffset(m_TotalVertexCount);
            mesh.SetFirstIndex(m_TotalIndexCount);
            m_TotalVertexCount += static_cast<uint32_t>(mesh.GetPositions().size());
            m_TotalIndexCount  += static_cast<uint32_t>(mesh.GetIndices().size());

            LOGINFO("Loaded mesh data: ", path);
            m_FilePaths.push_back(path);
            m_Meshes.push_back(std::move(mesh));
            m_Versions.push_back(0);
        }
    }

    std::optional<size_t> MeshContainer::FindMesh(const std::string& path) const
    {
        const auto found = std::find(m_FilePaths.begin(), m_FilePaths.end(), path);
        if (found == m_FilePaths.end())
            return std::nullopt;
        return static_cast<size_t>(found - m_FilePaths.begin());
    }

    bool MeshContainer::ReplaceGeometry(size_t index, TerrainMesh geometry)
    {
        Mesh& mesh = m_Meshes[index];
        if (geometry.Positions.size() != mesh.GetPositions().size() || geometry.Indices.size() != mesh.GetIndices().size())
            return false;
        const uint32_t firstIndex   = mesh.GetFirstIndex();
        const uint32_t vertexOffset = mesh.GetVertexOffset();
        mesh.SetGeometry(std::move(geometry.Positions), std::move(geometry.TexCoords), std::move(geometry.Normals),
                         std::move(geometry.Tangents), std::move(geometry.Indices));
        mesh.SetFirstIndex(firstIndex);
        mesh.SetVertexOffset(vertexOffset);
        ++m_Versions[index];
        return true;
    }

    uint32_t MeshContainer::GetMeshVersion(size_t index) const
    {
        return m_Versions[index];
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
