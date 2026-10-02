#pragma once

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ContentLoader
{
    // How a channel's value moves between two keys: held until the next key, or blended linearly
    // (a rotation along the shorter arc). A glTF CUBICSPLINE channel is imported as Linear.
    enum class AnimationInterpolation
    {
        Step,
        Linear,
    };

    // One animated property of a joint: key times in seconds, ascending, and one value per time.
    // Empty when the clip does not animate that property.
    template<typename T>
    struct LoadedKeys
    {
        std::vector<float>     Times;
        std::vector<T>         Values;
        AnimationInterpolation Interpolation = AnimationInterpolation::Linear;
    };

    // Every channel of a clip that targets one joint, in the joint's local space.
    struct LoadedJointChannel
    {
        uint32_t                       Joint = 0; // an index into LoadedMesh::Skin->Joints
        LoadedKeys<HM::Vector3>        Translation;
        LoadedKeys<HM::Quaternion>     Rotation;  // normalized
        LoadedKeys<HM::Vector3>        Scale;
    };

    struct LoadedAnimationClip
    {
        std::string                     Name;
        float                           Duration = 0.0f; // the latest key time of any channel
        std::vector<LoadedJointChannel> Channels;        // by joint index, one per animated joint
    };
}
