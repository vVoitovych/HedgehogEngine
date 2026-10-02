#include "api/Pose.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogAnimation
{
    namespace
    {
        HM::Vector3 Lerp(const HM::Vector3& a, const HM::Vector3& b, float t)
        {
            return a * (1.0f - t) + b * t;
        }

        HM::Quaternion Interpolate(const HM::Quaternion& a, const HM::Quaternion& b, float t)
        {
            return HM::Quaternion::Slerp(a, b, t);
        }

        HM::Vector3 Interpolate(const HM::Vector3& a, const HM::Vector3& b, float t)
        {
            return Lerp(a, b, t);
        }

        // The track's value at time: the first or last key outside its range, a key's own value
        // on it, and between two keys the earlier one (Step) or a blend of both (Linear).
        template<typename T>
        T Sample(const KeyTrack<T>& track, float time)
        {
            const std::vector<float>& times = track.Times;
            const auto                next  = std::upper_bound(times.begin(), times.end(), time);
            if (next == times.begin())
                return track.Values.front();
            if (next == times.end())
                return track.Values[times.size() - 1];

            const size_t key  = static_cast<size_t>(next - times.begin()) - 1;
            const float  span = times[key + 1] - times[key];
            const float  t    = span > 0.0f ? (time - times[key]) / span : 0.0f;
            if (track.Mode == Interpolation::Step || t <= 0.0f)
                return track.Values[key];
            return Interpolate(track.Values[key], track.Values[key + 1], t);
        }

        template<typename T>
        bool IsUsable(const KeyTrack<T>& track)
        {
            return !track.Times.empty() && track.Values.size() >= track.Times.size();
        }

        // The clip time a playback time maps to.
        float ClipTime(float time, float duration, bool loop)
        {
            if (!(duration > 0.0f) || !std::isfinite(time))
                return 0.0f;
            if (!loop)
                return std::clamp(time, 0.0f, duration);
            const float wrapped = std::fmod(time, duration);
            return wrapped < 0.0f ? wrapped + duration : wrapped;
        }
    }

    void SamplePose(const Skeleton& skeleton, const AnimationClip& clip, float time, bool loop,
                    std::vector<JointTransform>& outLocalPose)
    {
        outLocalPose.resize(skeleton.BindPose.size());
        std::copy(skeleton.BindPose.begin(), skeleton.BindPose.end(), outLocalPose.begin());

        const float clipTime = ClipTime(time, clip.Duration, loop);
        for (const JointTrack& track : clip.Tracks)
        {
            if (track.Joint >= outLocalPose.size())
                continue;
            JointTransform& joint = outLocalPose[track.Joint];
            if (IsUsable(track.Translation))
                joint.Translation = Sample(track.Translation, clipTime);
            if (IsUsable(track.Rotation))
                joint.Rotation = Sample(track.Rotation, clipTime);
            if (IsUsable(track.Scale))
                joint.Scale = Sample(track.Scale, clipTime);
        }
    }

    void BlendPoses(const std::vector<JointTransform>& a, const std::vector<JointTransform>& b, float weight,
                    std::vector<JointTransform>& outLocalPose)
    {
        const size_t count = std::min(a.size(), b.size());
        outLocalPose.resize(count);
        if (weight <= 0.0f || weight >= 1.0f)
        {
            const std::vector<JointTransform>& source = weight <= 0.0f ? a : b;
            std::copy(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(count), outLocalPose.begin());
            return;
        }

        for (size_t joint = 0; joint < count; ++joint)
        {
            const HM::Quaternion& from = a[joint].Rotation;
            HM::Quaternion        to   = b[joint].Rotation;
            if (HM::Dot(from, to) < 0.0f)
                to = HM::Quaternion(-to.x(), -to.y(), -to.z(), -to.w());
            const float keep = 1.0f - weight;

            outLocalPose[joint].Translation = Lerp(a[joint].Translation, b[joint].Translation, weight);
            outLocalPose[joint].Scale       = Lerp(a[joint].Scale, b[joint].Scale, weight);
            outLocalPose[joint].Rotation    = HM::Quaternion(from.x() * keep + to.x() * weight,
                                                             from.y() * keep + to.y() * weight,
                                                             from.z() * keep + to.z() * weight,
                                                             from.w() * keep + to.w() * weight)
                                               .Normalize();
        }
    }

    HM::Matrix4x4 ToMatrix(const JointTransform& transform)
    {
        return HM::Matrix4x4::GetTranslation(transform.Translation.x(), transform.Translation.y(),
                                             transform.Translation.z()) *
               transform.Rotation.ToMatrix() *
               HM::Matrix4x4::GetScale(transform.Scale.x(), transform.Scale.y(), transform.Scale.z());
    }

    void ComputeModelPose(const Skeleton& skeleton, const std::vector<JointTransform>& localPose,
                          std::vector<HM::Matrix4x4>& outModelPose)
    {
        const size_t count = std::min(skeleton.Parents.size(), localPose.size());
        outModelPose.resize(count);
        for (size_t joint = 0; joint < count; ++joint)
        {
            const int32_t parent = skeleton.Parents[joint];
            outModelPose[joint]  = parent >= 0 && static_cast<size_t>(parent) < joint
                                       ? outModelPose[parent] * ToMatrix(localPose[joint])
                                       : ToMatrix(localPose[joint]);
        }
    }

    void ComputeSkinningPalette(const std::vector<HM::Matrix4x4>& modelPose, const std::vector<HM::Matrix4x4>& inverseBind,
                                std::vector<HM::Matrix4x4>& outPalette)
    {
        const size_t count = std::min(modelPose.size(), inverseBind.size());
        outPalette.resize(count);
        for (size_t joint = 0; joint < count; ++joint)
            outPalette[joint] = modelPose[joint] * inverseBind[joint];
    }
}
