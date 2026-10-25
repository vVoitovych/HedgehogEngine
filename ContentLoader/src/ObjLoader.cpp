#include "ObjLoader.hpp"
#include "api/Tangents.hpp"
#include "Logger/api/Logger.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "ThirdParty/TinyObjectLoader/tiny_obj_loader.h"

#include <cmath>
#include <unordered_map>

namespace ContentLoader
{
    namespace
    {
        HM::Vector3 Cross(const HM::Vector3& a, const HM::Vector3& b)
        {
            return HM::Vector3(a.y() * b.z() - a.z() * b.y(), a.z() * b.x() - a.x() * b.z(),
                               a.x() * b.y() - a.y() * b.x());
        }

        // Gives every vertex without a normal (a zero one: a face corner with none, or a file's own
        // (0, 0, 0)) the sum of its triangles' face normals, each weighted by its area (the cross
        // product's length), normalized, so the triangles sharing it shade smoothly. One no triangle
        // gives a direction gets +Z.
        void GenerateMissingNormals(std::vector<LoadedVertexData>& vertices, const std::vector<uint32_t>& indices)
        {
            const HM::Vector3 zero(0.0f, 0.0f, 0.0f);
            std::vector<bool> needsNormal(vertices.size());
            for (size_t i = 0; i < vertices.size(); ++i)
                needsNormal[i] = vertices[i].normal == zero;

            for (size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const uint32_t    a = indices[i], b = indices[i + 1], c = indices[i + 2];
                const HM::Vector3 faceNormal = Cross(vertices[b].position - vertices[a].position,
                                                     vertices[c].position - vertices[a].position);
                for (const uint32_t vertex : { a, b, c })
                {
                    if (needsNormal[vertex])
                        vertices[vertex].normal = vertices[vertex].normal + faceNormal;
                }
            }

            for (size_t i = 0; i < vertices.size(); ++i)
            {
                if (!needsNormal[i])
                    continue;
                HM::Vector3& normal = vertices[i].normal;
                const float  length = std::sqrt(normal.x() * normal.x() + normal.y() * normal.y() + normal.z() * normal.z());
                normal = length > 1e-12f ? normal / length : HM::Vector3(0.0f, 0.0f, 1.0f);
            }
        }
    }

    std::optional<LoadedMesh> LoadObj(const std::string& path)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        {
            LOGERROR("Failed to load OBJ [", path, "]: ", warn, err);
            return std::nullopt;
        }

        std::unordered_map<LoadedVertexData, uint32_t> uniqueVertices{};
        LoadedMesh mesh;
        for (const auto& shape : shapes)
        {
            for (const auto& index : shape.mesh.indices)
            {
                LoadedVertexData vertex{};

                vertex.position =
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                if (index.texcoord_index >= 0)
                {
                    vertex.uv =
                    {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }
                else
                {
                    vertex.uv = { 0.0f, 0.0f };
                }

                // tinyobjloader gives -1 for a corner without a UV or a normal. One without a normal is
                // keyed by a zero normal, so corners at one position and UV share it, and gets a
                // generated normal once every face is read.
                if (index.normal_index >= 0)
                {
                    vertex.normal =
                    {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }
                else
                {
                    vertex.normal = { 0.0f, 0.0f, 0.0f };
                }

                if (uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back(vertex);
                }

                mesh.indices.push_back(uniqueVertices[vertex]);
            }
        }

        GenerateMissingNormals(mesh.vertices, mesh.indices);
        GenerateTangents(mesh.vertices, mesh.indices);

        LOGINFO("Model [", path, "] loaded with ", mesh.vertices.size(), " vertices and ", mesh.indices.size(), " indices!");

        return mesh;
    }
}
