#include "Mesh.hpp"
#include "ContentLoader/MeshLoader.hpp"
#include "Logger/Logger.hpp"

#include <unordered_map>

namespace Context
{
	void Mesh::LoadData(const std::string fileName)
	{
		ClearData();

		auto mesh = ContentLoader::LoadMesh(fileName);
		m_IndiciesData = mesh.indicies;
		m_Positions.reserve(mesh.verticies.size());
		m_TexCoords.reserve(mesh.verticies.size());
		m_Normals.reserve(mesh.verticies.size());
		for (size_t i = 0; i < mesh.verticies.size(); ++i)
		{
			m_Positions.push_back(mesh.verticies[i].position);
			m_Normals.push_back(mesh.verticies[i].normal);
			m_TexCoords.push_back(mesh.verticies[i].uv);
		}

		m_IndexCount = static_cast<uint32_t>(m_IndiciesData.size());

		LOGINFO("Model [", fileName,"] loaded with ", m_Positions.size(), " verticies and ", m_IndiciesData.size(), " indicies!");
	}

	void Mesh::ClearData()
	{
		m_Positions.clear();
		m_TexCoords.clear();
		m_Normals.clear();
		m_IndiciesData.clear();
	}

	std::vector<HM::Vector3> Mesh::GetPositions() const
	{
		return m_Positions;
	}

	std::vector<HM::Vector2> Mesh::GetTexCoords() const
	{
		return m_TexCoords;
	}

	std::vector<HM::Vector3> Mesh::GetNormals() const
	{
		return m_Normals;
	}

	std::vector<uint32_t> Mesh::GetIndicies() const
	{
		return m_IndiciesData;
	}

	uint32_t Mesh::GetIndexCount() const
	{
		return m_IndexCount;
	}

	uint32_t Mesh::GetFirstIndex() const
	{
		return m_FirstIndex;
	}

	uint32_t Mesh::GetVertexOffset() const
	{
		return m_VertexOffset;
	}

	void Mesh::SetFirstIndex(uint32_t firstIndex)
	{
		m_FirstIndex = firstIndex;
	}

	void Mesh::SetVertexOffset(uint32_t offset)
	{
		m_VertexOffset = offset;
	}

}




