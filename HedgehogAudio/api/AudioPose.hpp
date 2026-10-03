#pragma once

#include "HedgehogAudio/api/HedgehogAudioApi.hpp"

#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

namespace HA
{
    // Where a listener or a sound is and which way it faces: the engine's convention, looking down
    // -Z with +Y up.
    struct AudioPose
    {
        HM::Vector3 Position = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Forward  = HM::Vector3(0.0f, 0.0f, -1.0f);
        HM::Vector3 Up       = HM::Vector3(0.0f, 1.0f, 0.0f);
    };

    // The pose of a world matrix (Matrix4x4 stores columns: the x, y and z axes, then the
    // translation): its translation, its -Z axis as forward and its +Y axis as up, both made unit
    // length, so scale does not matter. An axis scaled to nothing keeps the default direction.
    [[nodiscard]] HEDGEHOG_AUDIO_API AudioPose MakeAudioPose(const HM::Matrix4x4& world);
}
