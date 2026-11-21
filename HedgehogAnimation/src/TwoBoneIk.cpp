#include "HedgehogAnimation/api/TwoBoneIk.hpp"

#include "HedgehogAnimation/api/Pose.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogAnimation
{
    namespace
    {
        constexpr float RADIANS_TO_DEGREES = 57.2957795f;
        // How far short of straight (and of folded flat) the solved chain stays, of its length.
        constexpr float REACH_MARGIN = 1e-4f;
        constexpr float EPSILON      = 1e-6f;

        HM::Vector3 Position(const HM::Matrix4x4& model) { return HM::Vector3(model[3].x(), model[3].y(), model[3].z()); }

        // v made unit length; zero stays zero.
        HM::Vector3 Unit(const HM::Vector3& v)
        {
            const float length = v.LengthSlow();
            return length > EPSILON ? v * (1.0f / length) : HM::Vector3(0.0f, 0.0f, 0.0f);
        }

        // A turn of radians about axis; the identity for a zero axis.
        HM::Quaternion Turn(const HM::Vector3& axis, float radians)
        {
            const HM::Vector3 unit = Unit(axis);
            if (unit.LengthSqr() == 0.0f || !std::isfinite(radians))
                return HM::Quaternion::Identity();
            return HM::Quaternion::FromAxisAngle(unit, radians * RADIANS_TO_DEGREES);
        }

        // The joint's rotation in model space, its ancestors' local rotations composed.
        HM::Quaternion ModelRotation(const Skeleton& skeleton, const std::vector<JointTransform>& localPose, int32_t joint)
        {
            HM::Quaternion rotation = HM::Quaternion::Identity();
            for (int32_t at = joint; at >= 0; at = skeleton.Parents[static_cast<size_t>(at)])
                rotation = localPose[static_cast<size_t>(at)].Rotation * rotation;
            return rotation.Normalize();
        }

        // The least turn taking from's direction to to's; the identity for a zero vector.
        HM::Quaternion ShortestArc(const HM::Vector3& from, const HM::Vector3& to)
        {
            const HM::Vector3 u = Unit(from);
            const HM::Vector3 v = Unit(to);
            if (u.LengthSqr() == 0.0f || v.LengthSqr() == 0.0f)
                return HM::Quaternion::Identity();
            const float cosine = std::clamp(HM::Dot(u, v), -1.0f, 1.0f);
            HM::Vector3 axis   = HM::Cross(u, v);
            if (Unit(axis).LengthSqr() == 0.0f)
            {
                if (cosine > 0.0f)
                    return HM::Quaternion::Identity();
                // Opposite: half a turn about any axis across them.
                axis = HM::Cross(u, std::abs(u.x()) < 0.9f ? HM::Vector3(1.0f, 0.0f, 0.0f) : HM::Vector3(0.0f, 1.0f, 0.0f));
            }
            return Turn(axis, std::acos(cosine));
        }

        // Normalized lerp from a to b the short way round.
        HM::Quaternion Nlerp(const HM::Quaternion& a, const HM::Quaternion& b, float t)
        {
            const float sign = HM::Dot(a, b) < 0.0f ? -1.0f : 1.0f;
            return HM::Quaternion(a.x() + (sign * b.x() - a.x()) * t, a.y() + (sign * b.y() - a.y()) * t,
                                  a.z() + (sign * b.z() - a.z()) * t, a.w() + (sign * b.w() - a.w()) * t)
                .Normalize();
        }

        bool IsBelow(const Skeleton& skeleton, size_t joint, uint32_t ancestor)
        {
            for (int32_t at = static_cast<int32_t>(joint); at >= 0; at = skeleton.Parents[static_cast<size_t>(at)])
                if (static_cast<uint32_t>(at) == ancestor)
                    return true;
            return false;
        }
    }

    std::optional<TwoBoneChain> FindTwoBoneChain(const Skeleton& skeleton, uint32_t endJoint)
    {
        if (endJoint >= skeleton.Parents.size())
            return std::nullopt;
        const int32_t mid = skeleton.Parents[endJoint];
        if (mid < 0)
            return std::nullopt;
        const int32_t root = skeleton.Parents[static_cast<size_t>(mid)];
        if (root < 0)
            return std::nullopt;
        return TwoBoneChain{ static_cast<uint32_t>(root), static_cast<uint32_t>(mid), endJoint };
    }

    void SolveTwoBoneIk(const Skeleton& skeleton, const TwoBoneChain& chain, const HM::Vector3& target,
                        const std::optional<HM::Vector3>& pole, float weight, std::vector<JointTransform>& localPose,
                        std::vector<HM::Matrix4x4>& modelPose)
    {
        const size_t count = std::min({ skeleton.Parents.size(), localPose.size(), modelPose.size() });
        if (chain.Root >= count || chain.Mid >= count || chain.End >= count || !(weight > 0.0f))
            return;
        weight = std::min(weight, 1.0f);

        const HM::Vector3 a = Position(modelPose[chain.Root]);
        const HM::Vector3 b = Position(modelPose[chain.Mid]);
        const HM::Vector3 c = Position(modelPose[chain.End]);
        const float upper   = (b - a).LengthSlow();
        const float lower   = (c - b).LengthSlow();
        if (upper < EPSILON || lower < EPSILON)
            return;

        // The distance to reach, kept between folded and straight.
        const float margin  = REACH_MARGIN * (upper + lower);
        const float reach   = std::clamp((target - a).LengthSlow(), std::abs(upper - lower) + margin, upper + lower - margin);

        // Where the mid and end must go: the end on the line to the target at the reach, the mid
        // off that line towards the pole (else the chain's present bend, else any side) at the angle
        // the law of cosines gives the root.
        const HM::Vector3 toTarget = target - a;
        HM::Vector3       line     = Unit(toTarget);
        if (line.LengthSqr() == 0.0f)
            line = Unit(c - a);
        const auto offLine = [&](const HM::Vector3& v) { return v - line * HM::Dot(v, line); };
        HM::Vector3 side = pole ? Unit(offLine(*pole - a)) : HM::Vector3(0.0f, 0.0f, 0.0f);
        if (side.LengthSqr() == 0.0f)
            side = Unit(offLine(b - a));
        if (side.LengthSqr() == 0.0f)
            side = Unit(offLine(std::abs(line.x()) < 0.9f ? HM::Vector3(1.0f, 0.0f, 0.0f) : HM::Vector3(0.0f, 1.0f, 0.0f)));
        const float       cosRoot  = std::clamp((upper * upper + reach * reach - lower * lower) / (2.0f * upper * reach), -1.0f, 1.0f);
        const float       sinRoot  = std::sqrt(std::max(0.0f, 1.0f - cosRoot * cosRoot));
        const HM::Vector3 midGoal  = a + line * (upper * cosRoot) + side * (upper * sinRoot);
        const HM::Vector3 endGoal  = a + line * reach;

        // The upper bone turned onto its new direction, then the lower one, carried by that, onto its.
        const HM::Quaternion chainTurn = ShortestArc(b - a, midGoal - a);
        const HM::Quaternion midTurn   = ShortestArc(chainTurn * (c - b), endGoal - midGoal);

        // The new model rotations, as local ones under each joint's parent.
        const int32_t        rootParent    = skeleton.Parents[chain.Root];
        const HM::Quaternion parentRotation = rootParent >= 0 ? ModelRotation(skeleton, localPose, rootParent)
                                                              : HM::Quaternion::Identity();
        const HM::Quaternion rootModel = ModelRotation(skeleton, localPose, static_cast<int32_t>(chain.Root));
        const HM::Quaternion midModel  = ModelRotation(skeleton, localPose, static_cast<int32_t>(chain.Mid));
        const HM::Quaternion rootNew   = (chainTurn * rootModel).Normalize();
        const HM::Quaternion midNew    = (midTurn * chainTurn * midModel).Normalize();
        const HM::Quaternion rootLocal = (parentRotation.Inverse() * rootNew).Normalize();
        const HM::Quaternion midLocal  = (rootNew.Inverse() * midNew).Normalize();

        localPose[chain.Root].Rotation = Nlerp(localPose[chain.Root].Rotation, rootLocal, weight);
        localPose[chain.Mid].Rotation  = Nlerp(localPose[chain.Mid].Rotation, midLocal, weight);

        // The root's subtree's model matrices again, parents first.
        for (size_t joint = chain.Root; joint < count; ++joint)
        {
            if (!IsBelow(skeleton, joint, chain.Root))
                continue;
            const int32_t parent = skeleton.Parents[joint];
            modelPose[joint]     = parent >= 0 ? modelPose[static_cast<size_t>(parent)] * ToMatrix(localPose[joint])
                                               : ToMatrix(localPose[joint]);
        }
    }
}
