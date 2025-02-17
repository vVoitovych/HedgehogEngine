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
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {
				LoadedVertexData vertex{};

				vertex.position =
				{
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.uv =
				{
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
				};

				vertex.normal =
				{
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(mesh.verticies.size());
					mesh.verticies.push_back(vertex);
					
				}

				mesh.indicies.push_back(uniqueVertices[vertex]);
			}
		}

		LOGINFO("Model [", path, "] loaded with ", mesh.verticies.size(), " verticies and ", mesh.indicies.size(), " indicies!");

		return mesh;
	}
}


