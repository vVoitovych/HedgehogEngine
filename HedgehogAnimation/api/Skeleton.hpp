#pragma once

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ContentLoader
{
    struct LoadedSkin;
}

namespace HedgehogAnimation
{
    // One joint's transform relative to its parent joint.
    struct JointTransform
    {
        HM::Vector3    Translation = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Quaternion Rotation    = HM::Quaternion::Identity();
        HM::Vector3    Scale       = HM::Vector3(1.0f, 1.0f, 1.0f);

        bool operator==(const JointTransform& other) const = default;
    };

    // The joints of a skinned mesh, ordered parent before child: Parents[j] is -1 or less than j.
    // Every array has one entry per joint.
    struct Skeleton
    {
        std::vector<std::string>    JointNames;
        std::vector<int32_t>        Parents;
        std::vector<HM::Matrix4x4>  InverseBind; // model space to the joint's space in the bind pose
        std::vector<JointTransform> BindPose;    // local transforms in the bind pose
    };

    [[nodiscard]] size_t GetJointCount(const Skeleton& skeleton);

    // The skeleton of a loaded skin; its joints are already ordered parent before child.
    [[nodiscard]] Skeleton BuildSkeleton(const ContentLoader::LoadedSkin& skin);
}
