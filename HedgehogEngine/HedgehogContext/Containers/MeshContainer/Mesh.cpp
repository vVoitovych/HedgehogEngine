#include "Mesh.hpp"
#include "ContentLoader/MeshLoader/MeshLoader.hpp"
#include "Logger/Logger.hpp"

#include <unordered_map>

namespace Context
{
	void Mesh::LoadData(const std::string fileName)
	{
		ClearData();

		auto mesh = ContentLoader::LoadMesh(fileName);
		mIndiciesData = mesh.indicies;
		mVerticiesData.reserve(mesh.verticies.size());
		for (size_t i = 0; i < mesh.verticies.size(); ++i)
		{
			Context::VertexDescription vertex;

			vertex.pos = mesh.verticies[i].position;
			vertex.normal = mesh.verticies[i].normal;
			vertex.texCoord = mesh.verticies[i].uv;
			mVerticiesData.push_back(vertex);
		}

		mIndexCount = static_cast<uint32_t>(mIndiciesData.size());

		LOGINFO("Model [", fileName,"] loaded with ", mVerticiesData.size(), " verticies and ", mIndiciesData.size(), " indicies!");
	}

	void Mesh::ClearData()
	{
		mVerticiesData.clear();
		mIndiciesData.clear();
	}

	std::vector<VertexDescription> Mesh::GetVerticies() const
	{
		return mVerticiesData;
	}

	std::vector<uint32_t> Mesh::GetIndicies() const
	{
		return mIndiciesData;
	}

	uint32_t Mesh::GetIndexCount() const
	{
		return mIndexCount;
	}

	uint32_t Mesh::GetFirstIndex() const
	{
		return mFirstIndex;
	}

	uint32_t Mesh::GetVertexOffset() const
	{
		return mVertexOffset;
	}

	void Mesh::SetFirstIndex(uint32_t firstIndex)
	{
		mFirstIndex = firstIndex;
	}

	void Mesh::SetVertexOffset(uint32_t offset)
	{
		mVertexOffset = offset;
	}

}




