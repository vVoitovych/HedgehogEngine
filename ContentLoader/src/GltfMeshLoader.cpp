#include "GltfMeshLoader.hpp"
#include "Logger/api/Logger.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/tinygltf/tiny_gltf.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <set>

namespace ContentLoader
{

namespace
{
    // One component of an accessor element as a double: floats as they are, integers as their
    // value, or scaled to [0, 1] when the accessor is normalized.
    double ReadComponent(const unsigned char* data, int componentType, bool normalized)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
        {
            float value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return normalized ? data[0] / 255.0 : data[0];
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            uint16_t value;
            std::memcpy(&value, data, sizeof(value));
            return normalized ? value / 65535.0 : value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
            uint32_t value;
            std::memcpy(&value, data, sizeof(value));
            return value;
        }
        default:
            return 0.0;
        }
    }

    // Reads every element of an accessor as `components` values of T, honouring the buffer view's
    // stride and the accessor's component type. An accessor with no buffer view reads as zeros,
    // as glTF specifies. Returns false and sets error when the accessor does not fit its buffer or
    // has another element type.
    template<typename T>
    bool ReadAccessor(const tinygltf::Model& model, int accessorIndex, int components, std::vector<T>& output,
                      std::string& error)
    {
        const std::string name = "accessor " + std::to_string(accessorIndex);
        if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        {
            error = name + " does not exist";
            return false;
        }
        const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
        const int actualComponents = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
        if (actualComponents != components)
        {
            error = name + " has " + std::to_string(actualComponents) + " components, expected " +
                    std::to_string(components);
            return false;
        }

        output.assign(accessor.count * components, T{});
        if (accessor.bufferView < 0)
            return true;
        if (accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
        {
            error = name + " refers to a missing buffer view";
            return false;
        }
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
        {
            error = name + " refers to a missing buffer";
            return false;
        }
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const int componentSize = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
        const int stride        = accessor.ByteStride(bufferView);
        if (componentSize <= 0 || stride <= 0)
        {
            error = name + " has an unsupported component type";
            return false;
        }
        const size_t start       = bufferView.byteOffset + accessor.byteOffset;
        const size_t elementSize = static_cast<size_t>(componentSize) * components;
        if (accessor.count > 0 &&
            start + (accessor.count - 1) * static_cast<size_t>(stride) + elementSize > buffer.data.size())
        {
            error = name + " reads past the end of its buffer";
            return false;
        }

        for (size_t element = 0; element < accessor.count; ++element)
        {
            const unsigned char* data = buffer.data.data() + start + element * stride;
            for (int component = 0; component < components; ++component)
                output[element * components + component] = static_cast<T>(
                    ReadComponent(data + component * componentSize, accessor.componentType, accessor.normalized));
        }
        return true;
    }

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

    std::string JointLabel(const tinygltf::Model& model, int node)
    {
        const std::string& name = model.nodes[node].name;
        return (name.empty() ? std::string("unnamed") : "'" + name + "'") + " (node " + std::to_string(node) + ")";
    }

    // Every node reachable from the default scene (or the first one). Empty when the file has no
    // scene, in which case every node counts as in the scene.
    std::set<int> SceneNodes(const tinygltf::Model& model)
    {
        std::set<int> nodes;
        if (model.scenes.empty())
            return nodes;
        const size_t     scene   = model.defaultScene >= 0 ? static_cast<size_t>(model.defaultScene) : 0;
        std::vector<int> pending = model.scenes[std::min(scene, model.scenes.size() - 1)].nodes;
        while (!pending.empty())
        {
            const int node = pending.back();
            pending.pop_back();
            if (node < 0 || node >= static_cast<int>(model.nodes.size()) || !nodes.insert(node).second)
                continue;
            pending.insert(pending.end(), model.nodes[node].children.begin(), model.nodes[node].children.end());
        }
        return nodes;
    }

    // Builds the skin with its joints reordered parent before child, and fills remap with each
    // glTF joint index's new index. Logs and returns nullopt for a malformed skin.
    std::optional<LoadedSkin> LoadSkin(const tinygltf::Model& model, const tinygltf::Skin& skin,
                                       const std::string& path, std::vector<uint32_t>& remap)
    {
        const std::vector<int>& joints = skin.joints;
        if (joints.empty())
        {
            LOGERROR("Skin of GLTF [" + path + "] has no joints");
            return std::nullopt;
        }

        const std::set<int> sceneNodes = SceneNodes(model);
        std::vector<int>    nodeParent(model.nodes.size(), -1);
        for (size_t node = 0; node < model.nodes.size(); ++node)
            for (const int child : model.nodes[node].children)
                if (child >= 0 && child < static_cast<int>(model.nodes.size()))
                    nodeParent[child] = static_cast<int>(node);

        std::vector<int> jointOfNode(model.nodes.size(), -1);
        for (size_t joint = 0; joint < joints.size(); ++joint)
        {
            const int node = joints[joint];
            if (node < 0 || node >= static_cast<int>(model.nodes.size()))
            {
                LOGERROR("Skin joint " + std::to_string(joint) + " of GLTF [" + path + "] refers to node " +
                         std::to_string(node) + ", which does not exist");
                return std::nullopt;
            }
            if (!sceneNodes.empty() && !sceneNodes.contains(node))
            {
                LOGERROR("Skin joint " + JointLabel(model, node) + " of GLTF [" + path + "] is not in the scene");
                return std::nullopt;
            }
            jointOfNode[node] = static_cast<int>(joint);
        }

        // Each joint's parent is its nearest ancestor that is also a joint. The walk is bounded,
        // so a file whose nodes form a cycle fails rather than hangs.
        std::vector<int> jointParent(joints.size(), -1);
        for (size_t joint = 0; joint < joints.size(); ++joint)
        {
            int node = nodeParent[joints[joint]];
            for (size_t steps = 0; node >= 0 && jointOfNode[node] < 0; ++steps)
            {
                if (steps > model.nodes.size())
                {
                    LOGERROR("Skin joint " + JointLabel(model, joints[joint]) + " of GLTF [" + path +
                             "] has a cycle among its ancestors");
                    return std::nullopt;
                }
                node = nodeParent[node];
            }
            jointParent[joint] = node >= 0 ? jointOfNode[node] : -1;
        }

        // Parents first: a joint is placed once its parent is, and each pass places at least one.
        std::vector<int> order;
        std::vector<int> placedAt(joints.size(), -1);
        while (order.size() < joints.size())
        {
            const size_t before = order.size();
            for (size_t joint = 0; joint < joints.size(); ++joint)
            {
                if (placedAt[joint] >= 0 || (jointParent[joint] >= 0 && placedAt[jointParent[joint]] < 0))
                    continue;
                placedAt[joint] = static_cast<int>(order.size());
                order.push_back(static_cast<int>(joint));
            }
            if (order.size() == before)
            {
                LOGERROR("Skin of GLTF [" + path + "] has joints that are their own ancestors");
                return std::nullopt;
            }
        }

        std::vector<float> inverseBind;
        if (skin.inverseBindMatrices >= 0)
        {
            std::string error;
            if (!ReadAccessor(model, skin.inverseBindMatrices, 16, inverseBind, error) ||
                inverseBind.size() < joints.size() * 16)
            {
                LOGERROR("Inverse bind matrices of GLTF [" + path + "] cannot be read: " +
                         (error.empty() ? std::string("fewer matrices than joints") : error));
                return std::nullopt;
            }
        }

        LoadedSkin loaded;
        remap.assign(joints.size(), 0);
        for (const int joint : order)
        {
            const tinygltf::Node& node = model.nodes[joints[joint]];
            LoadedJoint           loadedJoint;
            loadedJoint.Name   = node.name;
            loadedJoint.Parent = jointParent[joint] >= 0 ? placedAt[jointParent[joint]] : -1;
            if (!inverseBind.empty())
                for (size_t row = 0; row < 4; ++row)
                    for (size_t column = 0; column < 4; ++column)
                        loadedJoint.InverseBindMatrix[row][column] = inverseBind[joint * 16 + row * 4 + column];
            ReadNodeTransform(node, loadedJoint);
            remap[joint] = static_cast<uint32_t>(loaded.Joints.size());
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

    // The skin the file's meshes use: the one on the first node with both a mesh and a skin.
    // Fills skinnedMeshes with every mesh a node binds to that skin.
    int FindSkin(const tinygltf::Model& model, const std::string& path, std::set<int>& skinnedMeshes)
    {
        int  skin          = -1;
        bool warnedAnother = false;
        for (const tinygltf::Node& node : model.nodes)
        {
            if (node.mesh < 0 || node.skin < 0 || node.skin >= static_cast<int>(model.skins.size()))
                continue;
            if (skin < 0)
                skin = node.skin;
            if (node.skin == skin)
            {
                skinnedMeshes.insert(node.mesh);
            }
            else if (!warnedAnother)
            {
                LOGWARNING("GLTF [" + path + "] uses more than one skin; only the first is loaded");
                warnedAnother = true;
            }
        }
        return skin;
    }

    bool HasExtension(const std::string& path, const std::string& extension)
    {
        std::string actual = std::filesystem::path(path).extension().string();
        std::transform(actual.begin(), actual.end(), actual.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return actual == extension;
    }
}

    std::optional<LoadedMesh> LoadGltfMesh(const std::string& path)
    {
        tinygltf::Model    model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        const bool success = HasExtension(path, ".glb") ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
                                                        : loader.LoadASCIIFromFile(&model, &err, &warn, path);
        if (!success)
        {
            LOGERROR("Failed to load GLTF [", path, "]: ", err, " ", warn);
            return std::nullopt;
        }

        LoadedMesh meshData;

        std::set<int>         skinnedMeshes;
        std::vector<uint32_t> jointRemap;
        const int             skinIndex = FindSkin(model, path, skinnedMeshes);
        if (skinIndex >= 0)
        {
            meshData.Skin = LoadSkin(model, model.skins[skinIndex], path, jointRemap);
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

                if (meshData.Skin &&
                    !LoadSkinning(model, primitive, skinnedMeshes.contains(static_cast<int>(meshIndex)), vertexCount,
                                  jointRemap, path, meshData))
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
