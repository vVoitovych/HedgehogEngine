#pragma once

#include "AnimationClip.hpp"
#include "Skeleton.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <vector>

// Root motion: how far a clip moves its root joint, taken out of the pose so the entity can move
// instead. Model space is the mesh's, glTF's Y up, so the ground is X and Z and yaw is about Y.
namespace HedgehogAnimation
{
    struct RootMotionDelta
    {
        HM::Vector3    Translation = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Quaternion Rotation    = HM::Quaternion::Identity();
    };

    // Which of the root's movement RemoveRootMotion takes out of the pose: by default the
    // horizontal translation and the yaw, the vertical (a jump, a crouch) staying in it.
    struct RootMotionMask
    {
        bool TranslationX = true;
        bool TranslationY = false;
        bool TranslationZ = true;
        bool Yaw          = true;
    };

    // The root joint's movement in model space from fromTime to toTime of clip (its parent joints
    // as they are in the bind pose). Without loop the times are clamped to the clip, so it stops at
    // the ends; with loop every wrap adds the clip's end-minus-start, so a walk cycle keeps going.
    // Nothing for a clip without a track of rootJoint, or a joint out of range.
    [[nodiscard]] RootMotionDelta ExtractRootMotion(const Skeleton& skeleton, const AnimationClip& clip, uint32_t rootJoint,
                                                    float fromTime, float toTime, bool loop);

    // Adds weight of delta to total: the translation scaled, the rotation folded in by its share of
    // weightSoFar (the weight total already holds), the short way round.
    void AccumulateRootMotion(RootMotionDelta& total, float& weightSoFar, const RootMotionDelta& delta, float weight);

    // Puts the root joint back at its bind pose's values on the masked axes (and its twist about Y
    // for Yaw), so the skin does not move twice once the entity carries the motion.
    void RemoveRootMotion(std::vector<JointTransform>& localPose, uint32_t rootJoint,
                          const std::vector<JointTransform>& bindPose, const RootMotionMask& mask = {});
}
