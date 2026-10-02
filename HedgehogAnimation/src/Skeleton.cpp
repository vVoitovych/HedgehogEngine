#include "api/Skeleton.hpp"
#include "api/AnimationClip.hpp"

#include "ContentLoader/api/LoadedAnimation.hpp"
#include "ContentLoader/api/LoadedData.hpp"

// Conversions from what ContentLoader reads into the runtime's own plain data.
namespace HedgehogAnimation
{
    namespace
    {
        Interpolation ToInterpolation(ContentLoader::AnimationInterpolation interpolation)
        {
            return interpolation == ContentLoader::AnimationInterpolation::Step ? Interpolation::Step
                                                                                : Interpolation::Linear;
        }

        template<typename T>
        KeyTrack<T> ToTrack(const ContentLoader::LoadedKeys<T>& keys)
        {
            return KeyTrack<T>{ keys.Times, keys.Values, ToInterpolation(keys.Interpolation) };
        }
    }

    size_t GetJointCount(const Skeleton& skeleton)
    {
        return skeleton.Parents.size();
    }

    Skeleton BuildSkeleton(const ContentLoader::LoadedSkin& skin)
    {
        Skeleton skeleton;
        for (const ContentLoader::LoadedJoint& joint : skin.Joints)
        {
            skeleton.JointNames.push_back(joint.Name);
            skeleton.Parents.push_back(joint.Parent);
            skeleton.InverseBind.push_back(joint.InverseBindMatrix);
            skeleton.BindPose.push_back(JointTransform{ joint.Translation, joint.Rotation, joint.Scale });
        }
        return skeleton;
    }

    AnimationClip BuildAnimationClip(const ContentLoader::LoadedAnimationClip& loaded)
    {
        AnimationClip clip;
        clip.Name     = loaded.Name;
        clip.Duration = loaded.Duration;
        for (const ContentLoader::LoadedJointChannel& channel : loaded.Channels)
            clip.Tracks.push_back(JointTrack{ channel.Joint, ToTrack(channel.Translation), ToTrack(channel.Rotation),
                                              ToTrack(channel.Scale) });
        return clip;
    }
}
