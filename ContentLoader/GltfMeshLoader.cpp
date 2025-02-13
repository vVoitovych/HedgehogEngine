#include "GltfMeshLoader.hpp"
#include "Logger/Logger.hpp"

#include "ThirdParty/tinygltf/tiny_gltf.h"

#include <stdexcept>

namespace ContentLoader
{
    void LoadPositionData(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<HM::Vector4>& output)
    {
        if (primitive.attributes.find("POSITION") == primitive.attributes.end())
            return;

        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        size_t count = accessor.count;

        for (size_t i = 0; i < count; ++i)
        {
            output.push_back(HM::Vector4(data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2], 1.0f));
        }
    }

    void LoadNormalData(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<HM::Vector4>& output)
    {
        if (primitive.attributes.find("NORMAL") == primitive.attributes.end())
            return;

        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        size_t count = accessor.count;

        for (size_t i = 0; i < count; ++i)
        {
            output.push_back(HM::Vector4(data[i * 3 + 0], data[i * 3 + 1], data[i * 3 + 2], 0.0f));
        }
    }

    void LoadUVData(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<HM::Vector2>& output)
    {
        if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
            return;

        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        size_t count = accessor.count;

        for (size_t i = 0; i < count; ++i)
        {
            output.push_back(HM::Vector2(data[i * 2 + 0], data[i * 2 + 1]));
        }
    }

    void LoadJointData(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<HM::Vector4u>& output)
    {
        if (primitive.attributes.find("JOINTS_0") == primitive.attributes.end())
            return;

        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("JOINTS_0")];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const uint32_t* data = reinterpret_cast<const uint32_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        size_t count = accessor.count;

        for (size_t i = 0; i < count; ++i)
        {
            output.push_back(HM::Vector4u(data[i * 4 + 0], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]));
        }
    }

    void LoadWeightData(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        std::vector<HM::Vector4>& output)
    {
        if (primitive.attributes.find("WEIGHTS_0") == primitive.attributes.end())
            return;

        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("WEIGHTS_0")];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        const float* data = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
        size_t count = accessor.count;

        for (size_t i = 0; i < count; ++i)
        {
            output.push_back(HM::Vector4(data[i * 4 + 0], data[i * 4 + 1], data[i * 4 + 2], data[i * 4 + 3]));
        }
    }
    LoadedMesh LoadGltfMesh(const std::string& path)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
        if (!success) 
        {
            throw std::runtime_error("Failed to load GLTF: " + err + "\n" + warn);
        }

        LoadedMesh meshData;

        for (const auto& gltfMesh : model.meshes) {
            for (const auto& primitive : gltfMesh.primitives) 
            {
                std::vector<HM::Vector4> positions;
                std::vector<HM::Vector4> normals;
                std::vector<HM::Vector2> texCoords;
                std::vector<HM::Vector4u> jointIndicies;
                std::vector<HM::Vector4> jointWeights;

                LoadPositionData(model, primitive, positions);
                LoadNormalData(model, primitive, normals);
                LoadUVData(model, primitive, texCoords);
                LoadJointData(model, primitive, jointIndicies);
                LoadWeightData(model, primitive, jointWeights);

                for (size_t i = 0; i < positions.size(); ++i)
                {
                    LoadedVertexData vertex;
                    vertex.position = positions[i];
                    vertex.normal = normals[i];
                    vertex.uv = texCoords[i];
                    vertex.jointIndex = jointIndicies[i];
                    vertex.jointWeight = jointWeights[i];

                    meshData.verticies.push_back(vertex);
                }

                // Extract indices
                if (primitive.indices >= 0) {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                    const void* dataPtr = &buffer.data[bufferView.byteOffset + accessor.byteOffset];
                    size_t indexCount = accessor.count;

                    switch (accessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        for (size_t i = 0; i < indexCount; ++i) {
                            meshData.indicies.push_back(static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(dataPtr)[i]));
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        for (size_t i = 0; i < indexCount; ++i) {
                            meshData.indicies.push_back(static_cast<uint32_t>(reinterpret_cast<const uint16_t*>(dataPtr)[i]));
                        }
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        for (size_t i = 0; i < indexCount; ++i) {
                            meshData.indicies.push_back(reinterpret_cast<const uint32_t*>(dataPtr)[i]);
                        }
                        break;
                    default:
                        throw std::runtime_error("Unsupported index component type.");
                    }
                }
            }
        }

        LOGINFO("Model [", path, "] loaded with ", meshData.verticies.size(), " verticies and ", meshData.indicies.size(), " indicies!");

        return meshData;
    }



}


