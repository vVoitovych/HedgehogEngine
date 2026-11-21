#include "HedgehogAnimation/api/RootMotion.hpp"

#include "HedgehogAnimation/api/Pose.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogAnimation
{
    namespace
    {
        const JointTrack* FindTrack(const AnimationClip& clip, uint32_t joint)
        {
            for (const JointTrack& track : clip.Tracks)
                if (track.Joint == joint)
                    return &track;
            return nullptr;
        }

        // The rotation's twist about Y (its swing removed).
        HM::Quaternion TwistAboutY(const HM::Quaternion& rotation)
        {
            const HM::Quaternion twist(0.0f, rotation.y(), 0.0f, rotation.w());
            return twist.Length() > 1e-6f ? twist.Normalize() : HM::Quaternion::Identity();
        }

        // The root's parents' bind-pose rotation and scale, which carry its local movement into model space.
        HM::Quaternion ParentRotation(const Skeleton& skeleton, uint32_t joint, HM::Vector3& scale)
        {
            HM::Quaternion rotation = HM::Quaternion::Identity();
            scale                   = HM::Vector3(1.0f, 1.0f, 1.0f);
            for (int32_t at = skeleton.Parents[joint]; at >= 0; at = skeleton.Parents[static_cast<size_t>(at)])
            {
                rotation = skeleton.BindPose[static_cast<size_t>(at)].Rotation * rotation;
                scale    = HM::Vector3(scale.x() * skeleton.BindPose[static_cast<size_t>(at)].Scale.x(),
                                       scale.y() * skeleton.BindPose[static_cast<size_t>(at)].Scale.y(),
                                       scale.z() * skeleton.BindPose[static_cast<size_t>(at)].Scale.z());
            }
            return rotation.Normalize();
        }

        // The movement between two clip times within one play-through.
        RootMotionDelta Between(const Skeleton& skeleton, const AnimationClip& clip, uint32_t joint, float from, float to)
        {
            const JointTransform a = SampleJoint(skeleton, clip, joint, from, false);
            const JointTransform b = SampleJoint(skeleton, clip, joint, to, false);
            return RootMotionDelta{ b.Translation - a.Translation, (b.Rotation * a.Rotation.Inverse()).Normalize() };
        }

        void Add(RootMotionDelta& total, const RootMotionDelta& more, float times = 1.0f)
        {
            total.Translation = total.Translation + more.Translation * times;
            for (int i = 0; i < static_cast<int>(std::abs(times)); ++i)
                total.Rotation = (times > 0.0f ? more.Rotation : more.Rotation.Inverse()) * total.Rotation;
            total.Rotation = total.Rotation.Normalize();
        }
    }

    RootMotionDelta ExtractRootMotion(const Skeleton& skeleton, const AnimationClip& clip, uint32_t rootJoint,
                                      float fromTime, float toTime, bool loop)
    {
        RootMotionDelta delta;
        const float     duration = clip.Duration;
        if (rootJoint >= skeleton.Parents.size() || !FindTrack(clip, rootJoint) || !(duration > 0.0f) ||
            !std::isfinite(fromTime) || !std::isfinite(toTime))
            return delta;

        if (!loop)
            delta = Between(skeleton, clip, rootJoint, std::clamp(fromTime, 0.0f, duration), std::clamp(toTime, 0.0f, duration));
        else
        {
            // Within the play-through each time falls in, plus a whole cycle per wrap between them.
            const float fromCycle = std::floor(fromTime / duration);
            const float toCycle   = std::floor(toTime / duration);
            delta = Between(skeleton, clip, rootJoint, fromTime - fromCycle * duration, toTime - toCycle * duration);
            if (toCycle != fromCycle)
                Add(delta, Between(skeleton, clip, rootJoint, 0.0f, duration), toCycle - fromCycle);
        }

        HM::Vector3          scale;
        const HM::Quaternion parent = ParentRotation(skeleton, rootJoint, scale);
        const HM::Vector3    local(delta.Translation.x() * scale.x(), delta.Translation.y() * scale.y(),
                                   delta.Translation.z() * scale.z());
        delta.Translation = parent * local;
        delta.Rotation    = (parent * delta.Rotation * parent.Inverse()).Normalize();
        return delta;
    }

    void AccumulateRootMotion(RootMotionDelta& total, float& weightSoFar, const RootMotionDelta& delta, float weight)
    {
        if (!(weight > 0.0f))
            return;
        total.Translation = total.Translation + delta.Translation * weight;
        const float share = weight / (weightSoFar + weight);
        HM::Quaternion to = delta.Rotation;
        if (HM::Dot(total.Rotation, to) < 0.0f)
            to = HM::Quaternion(-to.x(), -to.y(), -to.z(), -to.w());
        const HM::Quaternion& from = total.Rotation;
        total.Rotation = HM::Quaternion(from.x() + (to.x() - from.x()) * share, from.y() + (to.y() - from.y()) * share,
                                        from.z() + (to.z() - from.z()) * share, from.w() + (to.w() - from.w()) * share)
                             .Normalize();
        weightSoFar += weight;
    }

    void RemoveRootMotion(std::vector<JointTransform>& localPose, uint32_t rootJoint,
                          const std::vector<JointTransform>& bindPose, const RootMotionMask& mask)
    {
        if (rootJoint >= localPose.size() || rootJoint >= bindPose.size())
            return;
        JointTransform&       root = localPose[rootJoint];
        const JointTransform& bind = bindPose[rootJoint];
        root.Translation = HM::Vector3(mask.TranslationX ? bind.Translation.x() : root.Translation.x(),
                                       mask.TranslationY ? bind.Translation.y() : root.Translation.y(),
                                       mask.TranslationZ ? bind.Translation.z() : root.Translation.z());
        if (mask.Yaw)
        {
            // Keep the swing, put back the bind pose's twist about Y.
            const HM::Quaternion swing = root.Rotation * TwistAboutY(root.Rotation).Inverse();
            root.Rotation              = (swing * TwistAboutY(bind.Rotation)).Normalize();
        }
    }
}
