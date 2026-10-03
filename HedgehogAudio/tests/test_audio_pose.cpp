#include "doctest/doctest/doctest.h"

#include "HedgehogAudio/api/AudioPose.hpp"

namespace
{
    // A world matrix from its columns: the x, y and z axes, then the translation.
    HM::Matrix4x4 FromColumns(const HM::Vector3& x, const HM::Vector3& y, const HM::Vector3& z, const HM::Vector3& t)
    {
        HM::Matrix4x4 m;
        m[0] = HM::Vector4(x.x(), x.y(), x.z(), 0.0f);
        m[1] = HM::Vector4(y.x(), y.y(), y.z(), 0.0f);
        m[2] = HM::Vector4(z.x(), z.y(), z.z(), 0.0f);
        m[3] = HM::Vector4(t.x(), t.y(), t.z(), 1.0f);
        return m;
    }

    bool Near(const HM::Vector3& a, const HM::Vector3& b)
    {
        return std::abs(a.x() - b.x()) < 1e-5f && std::abs(a.y() - b.y()) < 1e-5f && std::abs(a.z() - b.z()) < 1e-5f;
    }

    const HM::Vector3 X(1.0f, 0.0f, 0.0f), Y(0.0f, 1.0f, 0.0f), Z(0.0f, 0.0f, 1.0f), O(0.0f, 0.0f, 0.0f);
}

TEST_CASE("Audio pose - the identity looks down -Z with +Y up at the origin")
{
    const HA::AudioPose pose = HA::MakeAudioPose(FromColumns(X, Y, Z, O));
    CHECK(Near(pose.Position, O));
    CHECK(Near(pose.Forward, HM::Vector3(0.0f, 0.0f, -1.0f)));
    CHECK(Near(pose.Up, Y));
}

TEST_CASE("Audio pose - translation, a turn about Y and scale")
{
    // Turned 90 degrees about +Y: x becomes -z and z becomes +x, so -Z (forward) becomes -X.
    // Scaled by 3, which the pose ignores.
    const HA::AudioPose pose =
        HA::MakeAudioPose(FromColumns(HM::Vector3(0.0f, 0.0f, -3.0f), HM::Vector3(0.0f, 3.0f, 0.0f),
                                      HM::Vector3(3.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 2.0f, 3.0f)));
    CHECK(Near(pose.Position, HM::Vector3(1.0f, 2.0f, 3.0f)));
    CHECK(Near(pose.Forward, HM::Vector3(-1.0f, 0.0f, 0.0f)));
    CHECK(Near(pose.Up, Y));
}

TEST_CASE("Audio pose - an axis scaled to nothing keeps the default direction")
{
    const HA::AudioPose pose = HA::MakeAudioPose(FromColumns(X, O, O, HM::Vector3(5.0f, 0.0f, 0.0f)));
    CHECK(Near(pose.Position, HM::Vector3(5.0f, 0.0f, 0.0f)));
    CHECK(Near(pose.Forward, HM::Vector3(0.0f, 0.0f, -1.0f)));
    CHECK(Near(pose.Up, Y));
}
