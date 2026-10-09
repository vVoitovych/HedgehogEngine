#pragma once

#include "ThirdParty/tinygltf/tiny_gltf.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

// What the glTF mesh and animation loaders share: reading the file, reading accessors, and the
// skin's joint order, so a mesh's vertex joints and a clip's channels name joints alike.
namespace ContentLoader
{
    // Loads a .gltf or (by extension) .glb file; logs and returns false on failure. Without
    // decodeImages the images are listed (their uri kept) but not decoded, which is all a reader of
    // materials needs.
    bool LoadGltfModel(const std::string& path, tinygltf::Model& model, bool decodeImages = true);

    // One component of an accessor element as a double: floats as they are, integers as their
    // value, or scaled to [0, 1] (signed types to [-1, 1]) when the accessor is normalized.
    double ReadComponent(const unsigned char* data, int componentType, bool normalized);

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

    // "'<name>' (node <n>)", or "unnamed (node <n>)".
    std::string NodeLabel(const tinygltf::Model& model, int node);

    // The skin the file's meshes use: the one on the first node with both a mesh and a skin, or
    // -1. Fills skinnedMeshes with every mesh a node binds to that skin, and warns once when a
    // node binds another skin.
    int FindSkin(const tinygltf::Model& model, const std::string& path, std::set<int>& skinnedMeshes);

    // A skin's joints in the engine's order, parent before child.
    struct GltfJointOrder
    {
        std::vector<int>      Order;       // glTF joint indices, parents first
        std::vector<int>      Parent;      // per glTF joint index: its parent's glTF joint index, or -1
        std::vector<uint32_t> Remap;       // per glTF joint index: its engine joint index
        std::vector<int>      JointOfNode; // per node: its glTF joint index, or -1
    };

    // Orders a skin's joints; each joint's parent is its nearest ancestor that is also a joint.
    // Logs and returns nullopt for a skin with no joints, a joint node that does not exist or is
    // outside the scene, or nodes forming a cycle.
    std::optional<GltfJointOrder> OrderSkinJoints(const tinygltf::Model& model, const tinygltf::Skin& skin,
                                                  const std::string& path);
}
