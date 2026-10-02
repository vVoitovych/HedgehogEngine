#include "api/AnimationLoader.hpp"

#include "GltfModel.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <set>

namespace ContentLoader
{
    namespace
    {
        // Reads one sampler's keys into keys and raises duration to its latest time. A CUBICSPLINE
        // sampler stores an in-tangent, a value and an out-tangent per key; only the values are
        // kept, played linearly.
        template<typename T, int COMPONENTS>
        bool ReadKeys(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler, LoadedKeys<T>& keys,
                      float& duration, std::string& error)
        {
            std::vector<float> times;
            std::vector<float> values;
            if (!ReadAccessor(model, sampler.input, 1, times, error) ||
                !ReadAccessor(model, sampler.output, COMPONENTS, values, error))
                return false;

            const bool   cubic    = sampler.interpolation == "CUBICSPLINE";
            const size_t perKey   = cubic ? 3 : 1;
            const size_t keyCount = times.size();
            if (values.size() < keyCount * perKey * COMPONENTS)
            {
                error = "accessor " + std::to_string(sampler.output) + " has fewer values than the " +
                        std::to_string(keyCount) + " key times";
                return false;
            }

            keys.Interpolation = sampler.interpolation == "STEP" ? AnimationInterpolation::Step
                                                                 : AnimationInterpolation::Linear;
            if (!times.empty())
                duration = std::max(duration, *std::max_element(times.begin(), times.end()));
            keys.Times = std::move(times);
            keys.Values.clear();
            for (size_t key = 0; key < keyCount; ++key)
            {
                const float* value = values.data() + (key * perKey + (cubic ? 1 : 0)) * COMPONENTS;
                if constexpr (COMPONENTS == 4)
                {
                    const HM::Quaternion rotation(value[0], value[1], value[2], value[3]);
                    keys.Values.push_back(rotation.Length() > 0.0f ? rotation.Normalize() : HM::Quaternion::Identity());
                }
                else
                {
                    keys.Values.push_back(HM::Vector3(value[0], value[1], value[2]));
                }
            }
            return true;
        }

        // One glTF animation as a clip; logs and returns nullopt when its data cannot be read.
        std::optional<LoadedAnimationClip> LoadClip(const tinygltf::Model& model, size_t index,
                                                    const GltfJointOrder& order, const std::string& path)
        {
            const tinygltf::Animation& animation = model.animations[index];
            LoadedAnimationClip        clip;
            clip.Name = animation.name.empty() ? "Animation " + std::to_string(index) : animation.name;

            std::map<uint32_t, LoadedJointChannel> channels;
            bool                                   warnedCubic = false;
            for (const tinygltf::AnimationChannel& channel : animation.channels)
            {
                const int node = channel.target_node;
                if (node < 0 || node >= static_cast<int>(order.JointOfNode.size()) || order.JointOfNode[node] < 0)
                    continue;
                const std::string& property = channel.target_path;
                if (property != "translation" && property != "rotation" && property != "scale")
                    continue;

                std::string error;
                if (channel.sampler < 0 || channel.sampler >= static_cast<int>(animation.samplers.size()))
                {
                    error = "a channel uses sampler " + std::to_string(channel.sampler) + ", which does not exist";
                }
                else
                {
                    const tinygltf::AnimationSampler& sampler = animation.samplers[channel.sampler];
                    if (sampler.interpolation == "CUBICSPLINE" && !warnedCubic)
                    {
                        LOGWARNING("Animation '" + clip.Name + "' of GLTF [" + path +
                                   "] uses CUBICSPLINE interpolation; it is played as LINEAR");
                        warnedCubic = true;
                    }

                    const uint32_t      joint  = order.Remap[order.JointOfNode[node]];
                    LoadedJointChannel& target = channels[joint];
                    target.Joint               = joint;
                    const bool read =
                        property == "rotation"
                            ? ReadKeys<HM::Quaternion, 4>(model, sampler, target.Rotation, clip.Duration, error)
                            : ReadKeys<HM::Vector3, 3>(model, sampler,
                                                       property == "translation" ? target.Translation : target.Scale,
                                                       clip.Duration, error);
                    if (read)
                        continue;
                }
                LOGERROR("Animation '" + clip.Name + "' of GLTF [" + path + "] cannot be read: " + error);
                return std::nullopt;
            }

            for (auto& [joint, channel] : channels)
                clip.Channels.push_back(std::move(channel));
            return clip;
        }

        bool IsGltf(const std::string& path)
        {
            std::string extension = std::filesystem::path(path).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension == ".gltf" || extension == ".glb";
        }
    }

    std::optional<std::vector<LoadedAnimationClip>> LoadAnimations(const std::string&           fileName,
                                                                   const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = "assets://" + fileName;
        const auto        physPath    = fileSystem.ResolvePhysical(virtualPath);
        if (!physPath)
        {
            LOGERROR("Cannot resolve animation path: " + virtualPath);
            return std::nullopt;
        }
        const std::string path = physPath->string();
        if (!IsGltf(path))
        {
            LOGERROR("Unsupported animation file format: " + virtualPath);
            return std::nullopt;
        }

        tinygltf::Model model;
        if (!LoadGltfModel(path, model))
            return std::nullopt;

        std::vector<LoadedAnimationClip> clips;
        std::set<int>                    skinnedMeshes;
        const int                        skin = FindSkin(model, path, skinnedMeshes);
        if (skin < 0)
        {
            if (!model.animations.empty())
                LOGWARNING("GLTF [" + path + "] has animations but no skinned mesh; no clips are loaded");
            return clips;
        }

        const std::optional<GltfJointOrder> order = OrderSkinJoints(model, model.skins[skin], path);
        if (!order)
            return std::nullopt;
        for (size_t index = 0; index < model.animations.size(); ++index)
        {
            std::optional<LoadedAnimationClip> clip = LoadClip(model, index, *order, path);
            if (!clip)
                return std::nullopt;
            clips.push_back(std::move(*clip));
        }
        return clips;
    }
}
