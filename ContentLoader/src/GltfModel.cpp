#include "GltfModel.hpp"

#include "Logger/api/Logger.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "ThirdParty/tinygltf/tiny_gltf.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

namespace ContentLoader
{
    namespace
    {
        bool HasExtension(const std::string& path, const std::string& extension)
        {
            std::string actual = std::filesystem::path(path).extension().string();
            std::transform(actual.begin(), actual.end(), actual.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return actual == extension;
        }

        // Every node reachable from the default scene (or the first one). Empty when the file has
        // no scene, in which case every node counts as in the scene.
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
    }

    bool LoadGltfModel(const std::string& path, tinygltf::Model& model)
    {
        tinygltf::TinyGLTF loader;
        std::string        err, warn;

        const bool success = HasExtension(path, ".glb") ? loader.LoadBinaryFromFile(&model, &err, &warn, path)
                                                        : loader.LoadASCIIFromFile(&model, &err, &warn, path);
        if (!success)
            LOGERROR("Failed to load GLTF [", path, "]: ", err, " ", warn);
        return success;
    }

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
        case TINYGLTF_COMPONENT_TYPE_BYTE:
        {
            const auto value = static_cast<int8_t>(data[0]);
            return normalized ? std::max(value / 127.0, -1.0) : value;
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return normalized ? data[0] / 255.0 : data[0];
        case TINYGLTF_COMPONENT_TYPE_SHORT:
        {
            int16_t value;
            std::memcpy(&value, data, sizeof(value));
            return normalized ? std::max(value / 32767.0, -1.0) : value;
        }
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

    std::string NodeLabel(const tinygltf::Model& model, int node)
    {
        const std::string& name = model.nodes[node].name;
        return (name.empty() ? std::string("unnamed") : "'" + name + "'") + " (node " + std::to_string(node) + ")";
    }

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

    std::optional<GltfJointOrder> OrderSkinJoints(const tinygltf::Model& model, const tinygltf::Skin& skin,
                                                  const std::string& path)
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

        GltfJointOrder result;
        result.JointOfNode.assign(model.nodes.size(), -1);
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
                LOGERROR("Skin joint " + NodeLabel(model, node) + " of GLTF [" + path + "] is not in the scene");
                return std::nullopt;
            }
            result.JointOfNode[node] = static_cast<int>(joint);
        }

        // The walk up is bounded, so a file whose nodes form a cycle fails rather than hangs.
        result.Parent.assign(joints.size(), -1);
        for (size_t joint = 0; joint < joints.size(); ++joint)
        {
            int node = nodeParent[joints[joint]];
            for (size_t steps = 0; node >= 0 && result.JointOfNode[node] < 0; ++steps)
            {
                if (steps > model.nodes.size())
                {
                    LOGERROR("Skin joint " + NodeLabel(model, joints[joint]) + " of GLTF [" + path +
                             "] has a cycle among its ancestors");
                    return std::nullopt;
                }
                node = nodeParent[node];
            }
            result.Parent[joint] = node >= 0 ? result.JointOfNode[node] : -1;
        }

        // Parents first: a joint is placed once its parent is, and each pass places at least one.
        std::vector<int> placedAt(joints.size(), -1);
        while (result.Order.size() < joints.size())
        {
            const size_t before = result.Order.size();
            for (size_t joint = 0; joint < joints.size(); ++joint)
            {
                if (placedAt[joint] >= 0 || (result.Parent[joint] >= 0 && placedAt[result.Parent[joint]] < 0))
                    continue;
                placedAt[joint] = static_cast<int>(result.Order.size());
                result.Order.push_back(static_cast<int>(joint));
            }
            if (result.Order.size() == before)
            {
                LOGERROR("Skin of GLTF [" + path + "] has joints that are their own ancestors");
                return std::nullopt;
            }
        }

        result.Remap.resize(joints.size());
        for (size_t index = 0; index < result.Order.size(); ++index)
            result.Remap[result.Order[index]] = static_cast<uint32_t>(index);
        return result;
    }
}
