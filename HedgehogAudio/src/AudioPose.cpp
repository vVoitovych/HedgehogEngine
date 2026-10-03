#include "HedgehogAudio/api/AudioPose.hpp"

namespace HA
{
    namespace
    {
        // Column `column` of world as a unit vector, scaled by sign; fallback when it has no length.
        HM::Vector3 Axis(const HM::Matrix4x4& world, size_t column, float sign, const HM::Vector3& fallback)
        {
            const HM::Vector4& axis = world[column];
            const HM::Vector3  v(axis.x() * sign, axis.y() * sign, axis.z() * sign);
            return v.LengthSqr() > 0.0f ? v.Normalize() : fallback;
        }
    }

    AudioPose MakeAudioPose(const HM::Matrix4x4& world)
    {
        AudioPose          pose;
        const HM::Vector4& translation = world[3];
        pose.Position = HM::Vector3(translation.x(), translation.y(), translation.z());
        pose.Forward  = Axis(world, 2, -1.0f, pose.Forward);
        pose.Up       = Axis(world, 1, 1.0f, pose.Up);
        return pose;
    }
}
