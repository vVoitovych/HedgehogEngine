#pragma once

#include "Skeleton.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// Two-bone inverse kinematics (an arm or a leg: root, mid and end joints), as free functions over
// caller-owned poses.
namespace HedgehogAnimation
{
    struct TwoBoneChain
    {
        uint32_t Root = 0; // the end joint's grandparent (a shoulder or hip)
        uint32_t Mid  = 0; // its parent (an elbow or knee)
        uint32_t End  = 0; // the joint placed on the target (a hand or foot)
    };

    // The chain ending at endJoint; none for a joint out of range or with fewer than two ancestors.
    [[nodiscard]] std::optional<TwoBoneChain> FindTwoBoneChain(const Skeleton& skeleton, uint32_t endJoint);

    // Turns the chain's root and mid joints so its end reaches target (model space): the bones keep
    // their lengths, a target out of reach (or too near) is clamped to a hair short of the straight
    // chain (or to the folded one), and the bend lies in the plane towards pole when one is given,
    // else in the plane the chain bends in now. The root and mid local rotations are rewritten,
    // blended with the originals by weight (0 changes nothing, 1 is the solution) the short way
    // round, and the model matrices of the root and every joint below it computed again. modelPose
    // must be localPose's (ComputeModelPose). Allocates nothing; a chain out of range changes nothing.
    void SolveTwoBoneIk(const Skeleton& skeleton, const TwoBoneChain& chain, const HM::Vector3& target,
                        const std::optional<HM::Vector3>& pole, float weight, std::vector<JointTransform>& localPose,
                        std::vector<HM::Matrix4x4>& modelPose);
}
