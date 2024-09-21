#include "Mesh.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "ThirdParty/TinyObjectLoader/tiny_obj_loader.h"

#include "Logger/Logger.hpp"
#include "ContentLoader/CommonFunctions.hpp"

#include <unordered_map>

namespace Context
{
	void Mesh::LoadData(const std::string fileName)
	{
		ClearData();

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::string modelPath = ContentLoader::GetAssetsDirectory() + fileName;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<VertexDescription, uint32_t> uniqueVertices{};

		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				VertexDescription vertex{};

				vertex.pos =
				{
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord =
				{
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.color = { 0.0f, 0.0f, 0.0f };
				vertex.normal =
				{
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(mVerticiesData.size());
					mVerticiesData.push_back(vertex);
				}

				mIndiciesData.push_back(uniqueVertices[vertex]);
			}
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




