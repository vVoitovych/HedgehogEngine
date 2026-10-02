#include "GltfMeshLoader.hpp"
#include "GltfModel.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace ContentLoader
{

namespace
{
    // The rotation of a pure rotation matrix in the engine's row-vector convention (row i is the
    // image of axis i).
    HM::Quaternion RotationFromMatrix(const HM::Matrix4x4& r)
    {
        // m(i, j) is the column-vector matrix, the transpose of r.
        const auto  m     = [&r](size_t i, size_t j) { return r[j][i]; };
        const float trace = m(0, 0) + m(1, 1) + m(2, 2);
        if (trace > 0.0f)
        {
            const float s = 0.5f / std::sqrt(trace + 1.0f);
            return HM::Quaternion((m(2, 1) - m(1, 2)) * s, (m(0, 2) - m(2, 0)) * s, (m(1, 0) - m(0, 1)) * s, 0.25f / s)
                .Normalize();
        }
        if (m(0, 0) > m(1, 1) && m(0, 0) > m(2, 2))
        {
            const float s = 2.0f * std::sqrt(1.0f + m(0, 0) - m(1, 1) - m(2, 2));
            return HM::Quaternion(0.25f * s, (m(0, 1) + m(1, 0)) / s, (m(0, 2) + m(2, 0)) / s, (m(2, 1) - m(1, 2)) / s)
                .Normalize();
        }
        if (m(1, 1) > m(2, 2))
        {
            const float s = 2.0f * std::sqrt(1.0f + m(1, 1) - m(0, 0) - m(2, 2));
            return HM::Quaternion((m(0, 1) + m(1, 0)) / s, 0.25f * s, (m(1, 2) + m(2, 1)) / s, (m(0, 2) - m(2, 0)) / s)
                .Normalize();
        }
        const float s = 2.0f * std::sqrt(1.0f + m(2, 2) - m(0, 0) - m(1, 1));
        return HM::Quaternion((m(0, 2) + m(2, 0)) / s, (m(1, 2) + m(2, 1)) / s, 0.25f * s, (m(1, 0) - m(0, 1)) / s)
            .Normalize();
    }

    // A node's local transform as translation, rotation and scale. A node given as a matrix is
    // decomposed (a mirrored node keeps positive scale).
    void ReadNodeTransform(const tinygltf::Node& node, LoadedJoint& joint)
    {
        if (node.matrix.size() == 16)
        {
            // glTF's column-major floats are the engine's row-major matrix, rows in order.
            HM::Matrix4x4 matrix = HM::Matrix4x4::GetIdentity();
            for (size_t row = 0; row < 4; ++row)
                for (size_t column = 0; column < 4; ++column)
                    matrix[row][column] = static_cast<float>(node.matrix[row * 4 + column]);

            joint.Translation      = HM::Vector3(matrix[3][0], matrix[3][1], matrix[3][2]);
            HM::Matrix4x4 rotation = HM::Matrix4x4::GetIdentity();
            float         scale[3];
            for (size_t axis = 0; axis < 3; ++axis)
            {
                const HM::Vector3 row(matrix[axis][0], matrix[axis][1], matrix[axis][2]);
                scale[axis] = row.LengthSlow();
                for (size_t column = 0; column < 3; ++column)
                    rotation[axis][column] = scale[axis] > 0.0f ? row[column] / scale[axis] : 0.0f;
            }
            joint.Scale    = HM::Vector3(scale[0], scale[1], scale[2]);
            joint.Rotation = RotationFromMatrix(rotation);
            return;
        }
        if (node.translation.size() == 3)
            joint.Translation = HM::Vector3(static_cast<float>(node.translation[0]),
                                            static_cast<float>(node.translation[1]),
                                            static_cast<float>(node.translation[2]));
        if (node.rotation.size() == 4)
            joint.Rotation = HM::Quaternion(static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]),
                                            static_cast<float>(node.rotation[2]), static_cast<float>(node.rotation[3]))
                                 .Normalize();
        if (node.scale.size() == 3)
            joint.Scale = HM::Vector3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]),
                                      static_cast<float>(node.scale[2]));
    }

    // Builds the skin in the engine's joint order. Logs and returns nullopt for a malformed skin.
    std::optional<LoadedSkin> LoadSkin(const tinygltf::Model& model, const tinygltf::Skin& skin,
                                       const GltfJointOrder& order, const std::string& path)
    {
        std::vector<float> inverseBind;
        if (skin.inverseBindMatrices >= 0)
        {
            std::string error;
            if (!ReadAccessor(model, skin.inverseBindMatrices, 16, inverseBind, error) ||
                inverseBind.size() < skin.joints.size() * 16)
            {
                LOGERROR("Inverse bind matrices of GLTF [" + path + "] cannot be read: " +
                         (error.empty() ? std::string("fewer matrices than joints") : error));
                return std::nullopt;
            }
        }

        LoadedSkin loaded;
        for (const int joint : order.Order)
        {
            const tinygltf::Node& node = model.nodes[skin.joints[joint]];
            LoadedJoint           loadedJoint;
            loadedJoint.Name   = node.name;
            loadedJoint.Parent = order.Parent[joint] >= 0 ? static_cast<int32_t>(order.Remap[order.Parent[joint]]) : -1;
            if (!inverseBind.empty())
                for (size_t row = 0; row < 4; ++row)
                    for (size_t column = 0; column < 4; ++column)
                        loadedJoint.InverseBindMatrix[row][column] = inverseBind[joint * 16 + row * 4 + column];
            ReadNodeTransform(node, loadedJoint);
            loaded.Joints.push_back(std::move(loadedJoint));
        }
        return loaded;
    }

    // Appends one primitive's joints and weights, in the skin's joint order, weights summing to 1.
    // A primitive without JOINTS_0/WEIGHTS_0 (or one not bound to the skin) binds every vertex to
    // joint 0, as does a vertex whose weights are all zero.
    bool LoadSkinning(const tinygltf::Model& model, const tinygltf::Primitive& primitive, bool bound,
                      size_t vertexCount, const std::vector<uint32_t>& remap, const std::string& path,
                      LoadedMesh& mesh)
    {
        std::vector<uint32_t> joints;
        std::vector<float>    weights;
        const auto            jointsAttribute  = primitive.attributes.find("JOINTS_0");
        const auto            weightsAttribute = primitive.attributes.find("WEIGHTS_0");
        if (bound && jointsAttribute != primitive.attributes.end() && weightsAttribute != primitive.attributes.end())
        {
            std::string error;
            if (!ReadAccessor(model, jointsAttribute->second, 4, joints, error) ||
                !ReadAccessor(model, weightsAttribute->second, 4, weights, error))
            {
                LOGERROR("Skinning data of GLTF [" + path + "] cannot be read: " + error);
                return false;
            }
        }

        for (size_t vertex = 0; vertex < vertexCount; ++vertex)
        {
            HM::Vector4u vertexJoints(0u, 0u, 0u, 0u);
            HM::Vector4  vertexWeights(1.0f, 0.0f, 0.0f, 0.0f);
            if ((vertex + 1) * 4 <= joints.size() && (vertex + 1) * 4 <= weights.size())
            {
                float sum = 0.0f;
                for (size_t slot = 0; slot < 4; ++slot)
                    sum += std::max(weights[vertex * 4 + slot], 0.0f);
                for (size_t slot = 0; slot < 4 && sum > 0.0f; ++slot)
                {
                    const float    weight = std::max(weights[vertex * 4 + slot], 0.0f) / sum;
                    const uint32_t joint  = joints[vertex * 4 + slot];
                    if (weight > 0.0f && joint >= remap.size())
                    {
                        LOGERROR("Vertex " + std::to_string(vertex) + " of GLTF [" + path + "] uses joint " +
                                 std::to_string(joint) + ", but the skin has " + std::to_string(remap.size()));
                        return false;
                    }
                    vertexJoints[slot]  = weight > 0.0f ? remap[joint] : 0u;
                    vertexWeights[slot] = weight;
                }
            }
            mesh.Joints.push_back(vertexJoints);
            mesh.Weights.push_back(vertexWeights);
        }
        return true;
    }
}

    std::optional<LoadedMesh> LoadGltfMesh(const std::string& path)
    {
        tinygltf::Model model;
        if (!LoadGltfModel(path, model))
            return std::nullopt;

        LoadedMesh meshData;

        std::set<int>                 skinnedMeshes;
        std::optional<GltfJointOrder> jointOrder;
        const int                     skinIndex = FindSkin(model, path, skinnedMeshes);
        if (skinIndex >= 0)
        {
            jointOrder = OrderSkinJoints(model, model.skins[skinIndex], path);
            if (!jointOrder)
                return std::nullopt;
            meshData.Skin = LoadSkin(model, model.skins[skinIndex], *jointOrder, path);
            if (!meshData.Skin)
                return std::nullopt;
        }

        for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
        {
            for (const auto& primitive : model.meshes[meshIndex].primitives)
            {
                const auto attribute = [&primitive](const char* name)
                {
                    const auto found = primitive.attributes.find(name);
                    return found == primitive.attributes.end() ? -1 : found->second;
                };
                if (attribute("POSITION") < 0)
                    continue;

                std::vector<float> positions;
                std::vector<float> normals;
                std::vector<float> texCoords;
                std::string        error;
                if (!ReadAccessor(model, attribute("POSITION"), 3, positions, error) ||
                    (attribute("NORMAL") >= 0 && !ReadAccessor(model, attribute("NORMAL"), 3, normals, error)) ||
                    (attribute("TEXCOORD_0") >= 0 && !ReadAccessor(model, attribute("TEXCOORD_0"), 2, texCoords, error)))
                {
                    LOGERROR("Vertex data of GLTF [" + path + "] cannot be read: " + error);
                    return std::nullopt;
                }

                // Indices are relative to the primitive, so each is offset by the vertices the
                // earlier primitives added.
                const size_t baseVertex  = meshData.vertices.size();
                const size_t vertexCount = positions.size() / 3;
                for (size_t i = 0; i < vertexCount; ++i)
                {
                    LoadedVertexData vertex;
                    vertex.position = HM::Vector3(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
                    if ((i + 1) * 3 <= normals.size())
                        vertex.normal = HM::Vector3(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
                    if ((i + 1) * 2 <= texCoords.size())
                        vertex.uv = HM::Vector2(texCoords[i * 2 + 0], texCoords[i * 2 + 1]);

                    meshData.vertices.push_back(vertex);
                }

                if (jointOrder &&
                    !LoadSkinning(model, primitive, skinnedMeshes.contains(static_cast<int>(meshIndex)), vertexCount,
                                  jointOrder->Remap, path, meshData))
                    return std::nullopt;

                if (primitive.indices >= 0)
                {
                    std::vector<uint32_t> indices;
                    if (!ReadAccessor(model, primitive.indices, 1, indices, error))
                    {
                        LOGERROR("Indices of GLTF [" + path + "] cannot be read: " + error);
                        return std::nullopt;
                    }
                    for (const uint32_t index : indices)
                        meshData.indices.push_back(static_cast<uint32_t>(baseVertex) + index);
                }
                else
                {
                    for (size_t i = 0; i < vertexCount; ++i)
                        meshData.indices.push_back(static_cast<uint32_t>(baseVertex + i));
                }
            }
        }

        LOGINFO("Model [", path, "] loaded with ", meshData.vertices.size(), " vertices and ", meshData.indices.size(), " indices!");

        return meshData;
    }

}
