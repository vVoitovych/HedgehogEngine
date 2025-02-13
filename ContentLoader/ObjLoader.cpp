#include "ObjLoader.hpp"
#include "Logger/Logger.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "ThirdParty/TinyObjectLoader/tiny_obj_loader.h"

#include <stdexcept>
#include <unordered_map>

namespace ContentLoader
{
	struct VertexDescription
	{
		HM::Vector3 pos;
		HM::Vector2 texCoord;
		HM::Vector3 normal;
	};
}

namespace std
{
	template<>
	struct hash<HM::Vector2>
	{
		size_t operator()(HM::Vector2 const& vector) const
		{
			return (
				(hash<float>()(vector.x()) ^
					(hash<float>()(vector.y()) << 1))
				);
		}
	};

	template<>
	struct hash<HM::Vector3>
	{
		size_t operator()(HM::Vector3 const& vector) const
		{
			return (
				(hash<float>()(vector.x()) ^
					(hash<float>()(vector.y()) << 1) ^
					(hash<float>()(vector.z())))
				);
		}
	};

	template<>
	struct hash<ContentLoader::VertexDescription>
	{
		size_t operator()(ContentLoader::VertexDescription const& vertex) const
		{
			return (
				(hash<HM::Vector3>()(vertex.pos) )^ 
				(hash<HM::Vector2>()(vertex.texCoord) << 1) ^
				(hash<HM::Vector3>()(vertex.normal))
					);
		}
	};
}

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

		std::unordered_map<VertexDescription, uint32_t> uniqueVertices{};
		LoadedMesh mesh;
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

				vertex.normal =
				{
					attrib.normals[3 * index.normal_index + 0],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]
				};

				if (uniqueVertices.count(vertex) == 0)
				{
					uniqueVertices[vertex] = static_cast<uint32_t>(mesh.mPositions.size());
					mesh.mPositions.push_back(vertex.pos);
					mesh.mTexCoords.push_back(vertex.texCoord);
					mesh.mNormals.push_back(vertex.normal);
					mesh.mJointIndices.push_back(HM::Vector4u());
					mesh.mJointWeights.push_back(HM::Vector4());
				}

				mesh.mIndicies.push_back(uniqueVertices[vertex]);
			}
		}

		LOGINFO("Model [", path, "] loaded with ", mesh.mPositions.size(), " verticies and ", mesh.mIndicies.size(), " indicies!");

		return mesh;
	}
}


