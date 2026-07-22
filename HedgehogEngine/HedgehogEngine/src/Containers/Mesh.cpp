#include "HedgehogEngine/api/Containers/Mesh.hpp"

#include "ContentLoader/api/MeshLoader.hpp"
#include "Logger/api/Logger.hpp"

#include <unordered_map>

namespace HedgehogEngine
{
    bool Mesh::LoadData(const std::string& fileName,
                         const FS::FileSystemManager& fileSystem)
    {
        ClearData();

        const auto mesh = ContentLoader::LoadMesh(fileName, fileSystem);
        if (!mesh)
        {
            LOGERROR("Failed to load mesh data: ", fileName);
            return false;
        }

        m_IndicesData = mesh->indices;
        m_Positions.reserve(mesh->vertices.size());
        m_TexCoords.reserve(mesh->vertices.size());
        m_Normals.reserve(mesh->vertices.size());
        for (size_t i = 0; i < mesh->vertices.size(); ++i)
        {
            m_Positions.push_back(mesh->vertices[i].position);
            m_Normals.push_back(mesh->vertices[i].normal);
            m_TexCoords.push_back(mesh->vertices[i].uv);
        }

        m_IndexCount = static_cast<uint32_t>(m_IndicesData.size());

        if (!m_Positions.empty())
        {
            m_LocalBounds = HM::AABB(m_Positions[0], m_Positions[0]);
            for (size_t i = 1; i < m_Positions.size(); ++i)
                m_LocalBounds.ExpandToInclude(m_Positions[i]);
        }

        LOGINFO("Model [", fileName, "] loaded with ", m_Positions.size(), " vertices and ", m_IndicesData.size(), " indices!");
        return true;
    }

    void Mesh::ClearData()
    {
        m_Positions.clear();
        m_TexCoords.clear();
        m_Normals.clear();
        m_IndicesData.clear();
        m_LocalBounds = HM::AABB();
        m_IndexCount = 0;
    }

    const std::vector<HM::Vector3>& Mesh::GetPositions() const { return m_Positions; }
    const std::vector<HM::Vector2>& Mesh::GetTexCoords() const { return m_TexCoords; }
    const std::vector<HM::Vector3>& Mesh::GetNormals()   const { return m_Normals; }
    const std::vector<uint32_t>&    Mesh::GetIndices()   const { return m_IndicesData; }

    const HM::AABB& Mesh::GetLocalBounds() const { return m_LocalBounds; }

    uint32_t Mesh::GetIndexCount()   const { return m_IndexCount; }
    uint32_t Mesh::GetFirstIndex()   const { return m_FirstIndex; }
    uint32_t Mesh::GetVertexOffset() const { return m_VertexOffset; }

    void Mesh::SetFirstIndex(uint32_t firstIndex)  { m_FirstIndex   = firstIndex; }
    void Mesh::SetVertexOffset(uint32_t offset)    { m_VertexOffset = offset; }
}
