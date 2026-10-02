#pragma once

#include "AnimationClip.hpp"
#include "Skeleton.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <vector>

// Pose evaluation, as free functions over caller-owned buffers: each output is resized to the
// skeleton's joint count, so once the buffers have that size, evaluating again allocates nothing.
namespace HedgehogAnimation
{
    // The local pose of clip at time seconds: the bind pose, with every animated property sampled
    // from its track. With loop the time wraps into [0, Duration), negative times included;
    // without it the time is clamped, holding the first or last key. A time exactly on a key
    // gives that key's value. Tracks naming a joint the skeleton lacks are ignored.
    void SamplePose(const Skeleton& skeleton, const AnimationClip& clip, float time, bool loop,
                    std::vector<JointTransform>& outLocalPose);

    // Blends two local poses of one skeleton: weight 0 gives a, 1 gives b, exactly. Translation and
    // scale are lerped; rotations are nlerped the short way round (b negated when the two lie in
    // opposite hemispheres).
    void BlendPoses(const std::vector<JointTransform>& a, const std::vector<JointTransform>& b, float weight,
                    std::vector<JointTransform>& outLocalPose);

    // A joint's local matrix, translation * rotation * scale, as TransformSystem builds one.
    [[nodiscard]] HM::Matrix4x4 ToMatrix(const JointTransform& transform);

    // Each joint's model-space matrix: its parent's model matrix times its local matrix, computed
    // parent first.
    void ComputeModelPose(const Skeleton& skeleton, const std::vector<JointTransform>& localPose,
                          std::vector<HM::Matrix4x4>& outModelPose);

    // The matrices that skin a vertex: each joint's model matrix times its inverse bind matrix. The
    // bind pose gives identities.
    void ComputeSkinningPalette(const std::vector<HM::Matrix4x4>& modelPose, const std::vector<HM::Matrix4x4>& inverseBind,
                                std::vector<HM::Matrix4x4>& outPalette);
}
