#pragma once

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ContentLoader
{
    struct LoadedAnimationClip;
}

namespace HedgehogAnimation
{
    enum class Interpolation
    {
        Step,   // hold each key until the next
        Linear, // lerp, or slerp for rotations
    };

    // Keys of one property: times in seconds, ascending, one value each. Empty when the property is
    // not animated, so the bind pose's value is used.
    template<typename T>
    struct KeyTrack
    {
        std::vector<float> Times;
        std::vector<T>     Values;
        Interpolation      Mode = Interpolation::Linear;
    };

    struct JointTrack
    {
        uint32_t                 Joint = 0; // an index into the Skeleton
        KeyTrack<HM::Vector3>    Translation;
        KeyTrack<HM::Quaternion> Rotation;
        KeyTrack<HM::Vector3>    Scale;
    };

    struct AnimationClip
    {
        std::string             Name;
        float                   Duration = 0.0f;
        std::vector<JointTrack> Tracks;
    };

    // The clip of a loaded animation, whose joints index the same file's skeleton.
    [[nodiscard]] AnimationClip BuildAnimationClip(const ContentLoader::LoadedAnimationClip& loaded);
}
