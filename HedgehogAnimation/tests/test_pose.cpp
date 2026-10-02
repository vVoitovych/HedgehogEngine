#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/Pose.hpp"

#include "ContentLoader/api/LoadedAnimation.hpp"
#include "ContentLoader/api/LoadedData.hpp"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <new>
#include <vector>

using namespace HedgehogAnimation;

// Counts global allocations while s_Counting is set, so a test can prove a span of code allocates
// nothing. This test executable owns the global operator new.
namespace
{
    std::atomic<bool>   s_Counting    = false;
    std::atomic<size_t> s_Allocations = 0;
}

void* operator new(size_t size)
{
    if (s_Counting)
        ++s_Allocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
    std::free(memory);
}

namespace
{
    constexpr float EPSILON = 1e-5f;

    bool Near(float a, float b) { return std::abs(a - b) < EPSILON; }
    bool Near(const HM::Vector3& a, const HM::Vector3& b) { return Near(a.x(), b.x()) && Near(a.y(), b.y()) && Near(a.z(), b.z()); }

    // The same rotation: q and -q are equal.
    bool SameRotation(const HM::Quaternion& a, const HM::Quaternion& b)
    {
        return std::abs(std::abs(HM::Dot(a, b)) - 1.0f) < EPSILON;
    }

    bool Near(const HM::Matrix4x4& a, const HM::Matrix4x4& b)
    {
        for (size_t column = 0; column < 4; ++column)
            for (size_t row = 0; row < 4; ++row)
                if (!Near(a[column][row], b[column][row]))
                    return false;
        return true;
    }

    HM::Quaternion AboutZ(float degrees) { return HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 0.0f, 1.0f), degrees); }

    // A chain Root -> Arm -> Hand, each joint one unit along x from its parent, Root turned 90
    // degrees about z and Arm scaled by 2. InverseBind is the bind pose's model matrix inverted.
    Skeleton MakeChain()
    {
        Skeleton skeleton;
        skeleton.JointNames = { "Root", "Arm", "Hand" };
        skeleton.Parents    = { -1, 0, 1 };
        skeleton.BindPose   = {
            JointTransform{ HM::Vector3(0.0f, 0.0f, 0.0f), AboutZ(90.0f), HM::Vector3(1.0f, 1.0f, 1.0f) },
            JointTransform{ HM::Vector3(1.0f, 0.0f, 0.0f), HM::Quaternion::Identity(), HM::Vector3(2.0f, 2.0f, 2.0f) },
            JointTransform{ HM::Vector3(1.0f, 0.0f, 0.0f), AboutZ(30.0f), HM::Vector3(1.0f, 1.0f, 1.0f) },
        };
        std::vector<HM::Matrix4x4> model;
        ComputeModelPose(skeleton, skeleton.BindPose, model);
        for (const HM::Matrix4x4& matrix : model)
            skeleton.InverseBind.push_back(matrix.Inverse());
        return skeleton;
    }

    // Arm's translation runs (0,0,0) -> (1,0,0) -> (3,0,0) over keys at 0, 1 and 2 s, its rotation
    // turns 0 -> 90 degrees about z over the first second, and its scale steps from 1 to 2 at 1 s.
    AnimationClip MakeClip()
    {
        JointTrack arm;
        arm.Joint       = 1;
        arm.Translation = { { 0.0f, 1.0f, 2.0f },
                            { HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(3.0f, 0.0f, 0.0f) },
                            Interpolation::Linear };
        arm.Rotation    = { { 0.0f, 1.0f }, { HM::Quaternion::Identity(), AboutZ(90.0f) }, Interpolation::Linear };
        arm.Scale       = { { 0.0f, 1.0f }, { HM::Vector3(1.0f, 1.0f, 1.0f), HM::Vector3(2.0f, 2.0f, 2.0f) },
                            Interpolation::Step };
        return AnimationClip{ "Wave", 2.0f, { arm } };
    }
}

TEST_CASE("SamplePose - keys exactly, interpolation between them, looping and clamping")
{
    const Skeleton              skeleton = MakeChain();
    const AnimationClip         clip     = MakeClip();
    std::vector<JointTransform> pose;

    SUBCASE("on a key the key's value, exactly")
    {
        SamplePose(skeleton, clip, 1.0f, false, pose);
        CHECK(pose[1].Translation == HM::Vector3(1.0f, 0.0f, 0.0f));
        CHECK(pose[1].Rotation == AboutZ(90.0f));
        CHECK(pose[1].Scale == HM::Vector3(2.0f, 2.0f, 2.0f));
        CHECK(pose[0] == skeleton.BindPose[0]); // not animated: the bind pose
        CHECK(pose[2] == skeleton.BindPose[2]);
    }
    SUBCASE("between keys: linear blends, step holds")
    {
        SamplePose(skeleton, clip, 0.5f, false, pose);
        CHECK(Near(pose[1].Translation, HM::Vector3(0.5f, 0.0f, 0.0f)));
        CHECK(SameRotation(pose[1].Rotation, AboutZ(45.0f)));
        CHECK(pose[1].Scale == HM::Vector3(1.0f, 1.0f, 1.0f));
        SamplePose(skeleton, clip, 1.5f, false, pose);
        CHECK(Near(pose[1].Translation, HM::Vector3(2.0f, 0.0f, 0.0f)));
    }
    SUBCASE("looping wraps, negative times included")
    {
        SamplePose(skeleton, clip, 2.5f, true, pose);
        CHECK(Near(pose[1].Translation, HM::Vector3(0.5f, 0.0f, 0.0f)));
        SamplePose(skeleton, clip, -0.5f, true, pose);
        CHECK(Near(pose[1].Translation, HM::Vector3(2.0f, 0.0f, 0.0f)));
    }
    SUBCASE("clamping holds the first and last keys")
    {
        SamplePose(skeleton, clip, 5.0f, false, pose);
        CHECK(pose[1].Translation == HM::Vector3(3.0f, 0.0f, 0.0f));
        CHECK(pose[1].Rotation == AboutZ(90.0f));
        SamplePose(skeleton, clip, -1.0f, false, pose);
        CHECK(pose[1].Translation == HM::Vector3(0.0f, 0.0f, 0.0f));
    }
    SUBCASE("a track for a joint the skeleton lacks is ignored")
    {
        AnimationClip stray = clip;
        stray.Tracks[0].Joint = 7;
        SamplePose(skeleton, stray, 1.0f, false, pose);
        CHECK(pose == skeleton.BindPose);
    }
}

TEST_CASE("BlendPoses - weights 0 and 1 give each input exactly, and rotations blend the short way")
{
    const Skeleton              skeleton = MakeChain();
    std::vector<JointTransform> a;
    std::vector<JointTransform> b;
    std::vector<JointTransform> blended;
    SamplePose(skeleton, MakeClip(), 0.3f, false, a);
    SamplePose(skeleton, MakeClip(), 1.7f, false, b);

    BlendPoses(a, b, 0.0f, blended);
    CHECK(blended == a);
    BlendPoses(a, b, 1.0f, blended);
    CHECK(blended == b);

    BlendPoses(a, b, 0.5f, blended);
    CHECK(Near(blended[1].Translation, (a[1].Translation + b[1].Translation) * 0.5f));

    // b's rotation is 90 degrees about z written as its negation, the opposite hemisphere; the
    // halfway blend must be 45 degrees, not the long way round.
    std::vector<JointTransform> identity(1);
    std::vector<JointTransform> quarter(1);
    const HM::Quaternion        turn = AboutZ(90.0f);
    quarter[0].Rotation              = HM::Quaternion(-turn.x(), -turn.y(), -turn.z(), -turn.w());
    BlendPoses(identity, quarter, 0.5f, blended);
    CHECK(SameRotation(blended[0].Rotation, AboutZ(45.0f)));
    CHECK(Near(blended[0].Rotation.Rotate(HM::Vector3(1.0f, 0.0f, 0.0f)), HM::Vector3(0.7071068f, 0.7071068f, 0.0f)));
}

TEST_CASE("ComputeModelPose - a child is its parent's matrix times its own, and the bind pose skins to identity")
{
    const Skeleton             skeleton = MakeChain();
    std::vector<HM::Matrix4x4> model;
    ComputeModelPose(skeleton, skeleton.BindPose, model);
    REQUIRE(model.size() == 3u);

    // Root turns 90 degrees about z, so Arm one unit along Root's x sits at (0, 1, 0), and Hand,
    // one unit along Arm's doubled x, at (0, 3, 0).
    const HM::Vector4 arm  = model[1] * HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    const HM::Vector4 hand = model[2] * HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(Near(HM::Vector3(arm.x(), arm.y(), arm.z()), HM::Vector3(0.0f, 1.0f, 0.0f)));
    CHECK(Near(HM::Vector3(hand.x(), hand.y(), hand.z()), HM::Vector3(0.0f, 3.0f, 0.0f)));
    CHECK(Near(model[2], model[1] * ToMatrix(skeleton.BindPose[2])));

    std::vector<HM::Matrix4x4> palette;
    ComputeSkinningPalette(model, skeleton.InverseBind, palette);
    REQUIRE(palette.size() == 3u);
    for (const HM::Matrix4x4& matrix : palette)
        CHECK(Near(matrix, HM::Matrix4x4::GetIdentity()));
}

TEST_CASE("Pose evaluation allocates nothing once the buffers are sized")
{
    const Skeleton              skeleton = MakeChain();
    const AnimationClip         clip     = MakeClip();
    std::vector<JointTransform> a;
    std::vector<JointTransform> b;
    std::vector<JointTransform> blended;
    std::vector<HM::Matrix4x4>  model;
    std::vector<HM::Matrix4x4>  palette;

    const auto evaluate = [&](float time)
    {
        SamplePose(skeleton, clip, time, true, a);
        SamplePose(skeleton, clip, time + 0.7f, true, b);
        BlendPoses(a, b, 0.25f, blended);
        ComputeModelPose(skeleton, blended, model);
        ComputeSkinningPalette(model, skeleton.InverseBind, palette);
    };
    evaluate(0.0f); // sizes the buffers

    // The counter sees an allocation, so a zero below means none happened.
    s_Allocations = 0;
    s_Counting    = true;
    {
        const std::vector<int> probe(4);
    }
    s_Counting = false;
    REQUIRE(s_Allocations >= 1u); // Debug builds add an iterator proxy

    s_Allocations = 0;
    s_Counting    = true;
    for (int frame = 0; frame < 120; ++frame)
        evaluate(frame / 60.0f);
    s_Counting = false;
    CHECK(s_Allocations == 0u);
    CHECK(palette.size() == 3u);
}

TEST_CASE("BuildSkeleton and BuildAnimationClip copy what ContentLoader read")
{
    ContentLoader::LoadedSkin skin;
    skin.Joints.push_back(ContentLoader::LoadedJoint{ "Root", -1, HM::Matrix4x4::GetTranslation(-1.0f, 0.0f, 0.0f),
                                                      HM::Vector3(1.0f, 0.0f, 0.0f), AboutZ(10.0f),
                                                      HM::Vector3(1.0f, 1.0f, 1.0f) });
    skin.Joints.push_back(ContentLoader::LoadedJoint{ "Tip", 0 });
    const Skeleton skeleton = BuildSkeleton(skin);
    REQUIRE(GetJointCount(skeleton) == 2u);
    CHECK(skeleton.JointNames[1] == "Tip");
    CHECK(skeleton.Parents == std::vector<int32_t>{ -1, 0 });
    CHECK(skeleton.InverseBind[0] == HM::Matrix4x4::GetTranslation(-1.0f, 0.0f, 0.0f));
    CHECK(skeleton.BindPose[0].Rotation == AboutZ(10.0f));

    ContentLoader::LoadedAnimationClip loaded;
    loaded.Name     = "Idle";
    loaded.Duration = 1.0f;
    ContentLoader::LoadedJointChannel channel;
    channel.Joint                   = 1;
    channel.Scale.Times             = { 0.0f, 1.0f };
    channel.Scale.Values            = { HM::Vector3(1.0f, 1.0f, 1.0f), HM::Vector3(2.0f, 2.0f, 2.0f) };
    channel.Scale.Interpolation     = ContentLoader::AnimationInterpolation::Step;
    loaded.Channels.push_back(channel);

    const AnimationClip clip = BuildAnimationClip(loaded);
    CHECK(clip.Name == "Idle");
    CHECK(clip.Duration == 1.0f);
    REQUIRE(clip.Tracks.size() == 1u);
    CHECK(clip.Tracks[0].Joint == 1u);
    CHECK(clip.Tracks[0].Scale.Mode == Interpolation::Step);
    CHECK(clip.Tracks[0].Scale.Values[1] == HM::Vector3(2.0f, 2.0f, 2.0f));
    CHECK(clip.Tracks[0].Translation.Times.empty());
}
