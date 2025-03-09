#include "ObjLoader.hpp"
#include "Logger/Logger.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "ThirdParty/TinyObjectLoader/tiny_obj_loader.h"

#include <stdexcept>
#include <unordered_map>

namespace ContentLoader
{
	LoadedMesh LoadObj(const std::string& path)
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<LoadedVertexData, uint32_t> uniqueVertices{};
		LoadedMesh mesh;
		for (const auto& shape : shapes) 
		{
			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) 
			{
				auto idx0 = shape.mesh.indices[i + 0];
				auto idx1 = shape.mesh.indices[i + 1];
				auto idx2 = shape.mesh.indices[i + 2];

				LoadedVertexData v0{}, v1{}, v2{};

				auto loadVertex = [&](LoadedVertexData& v, tinyobj::index_t idx) 
					{
						v.position = 
						{
						  attrib.vertices[3 * idx.vertex_index + 0],
						  attrib.vertices[3 * idx.vertex_index + 1],
						  attrib.vertices[3 * idx.vertex_index + 2]
						};

						v.uv = 
						{
						  attrib.texcoords[2 * idx.texcoord_index + 0],
						  1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
						};

						v.normal = 
						{
						  attrib.normals[3 * idx.normal_index + 0],
						  attrib.normals[3 * idx.normal_index + 1],
						  attrib.normals[3 * idx.normal_index + 2]
						};
					};

				loadVertex(v0, idx0);
				loadVertex(v1, idx1);
				loadVertex(v2, idx2);

				// Compute tangent and bitangent for the triangle
				HM::Vector3 edge1 = v1.position - v0.position;
				HM::Vector3 edge2 = v2.position - v0.position;

				HM::Vector2 deltaUV1 = v1.uv - v0.uv;
				HM::Vector2 deltaUV2 = v2.uv - v0.uv;

				float determinant = deltaUV1.x() * deltaUV2.y() - deltaUV2.x() * deltaUV1.y();
				float invDet = (determinant == 0.0f) ? 0.0f : (1.0f / determinant);

				HM::Vector3 tangent, bitangent;
				if (determinant != 0.0f) 
				{
					tangent = (edge1 * deltaUV2.y() - edge2 * deltaUV1.y()) * invDet;
					bitangent = (edge2 * deltaUV1.x() - edge1 * deltaUV2.x()) * invDet;
				}
				else 
				{
					tangent = HM::Vector3(1.0f, 0.0f, 0.0f); // Fallback
					bitangent = HM::Vector3(0.0f, 1.0f, 0.0f);
				}

				// Normalize tangent and bitangent
				tangent = tangent.Normalize();
				bitangent = bitangent.Normalize();

				auto addVertex = [&](LoadedVertexData& vertex) 
				{
					if (uniqueVertices.count(vertex) == 0)
					{
						uniqueVertices[vertex] = static_cast<uint32_t>(mesh.verticies.size());
						vertex.tangent = tangent;
						vertex.bitangent = bitangent;
						mesh.verticies.push_back(vertex);
					}
					mesh.indicies.push_back(uniqueVertices[vertex]);
				};

				addVertex(v0);
				addVertex(v1);
				addVertex(v2);
			}
		}

		LOGINFO("Model [", path, "] loaded with ", mesh.verticies.size(), " verticies and ", mesh.indicies.size(), " indicies!");

		return mesh;
	}
}


