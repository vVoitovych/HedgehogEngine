#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/BlendStack.hpp"
#include "HedgehogAnimation/api/RootMotion.hpp"

#include <array>
#include <cmath>
#include <vector>

using namespace HedgehogAnimation;

namespace
{
    constexpr float EPSILON = 1e-4f;

    bool Near(const HM::Vector3& a, const HM::Vector3& b) { return (a - b).LengthSlow() < EPSILON; }

    bool SameRotation(const HM::Quaternion& a, const HM::Quaternion& b)
    {
        return std::abs(std::abs(HM::Dot(a, b)) - 1.0f) < EPSILON;
    }

    HM::Quaternion AboutY(float degrees) { return HM::Quaternion::FromAxisAngle(HM::Vector3(0.0f, 1.0f, 0.0f), degrees); }

    // Root (joint 0) and a child Spine one unit up.
    Skeleton MakeSkeleton()
    {
        Skeleton skeleton;
        skeleton.JointNames = { "Root", "Spine" };
        skeleton.Parents    = { -1, 0 };
        skeleton.BindPose   = { JointTransform{}, JointTransform{ HM::Vector3(0.0f, 1.0f, 0.0f) } };
        return skeleton;
    }

    AnimationClip MakeTranslationClip(const char* name, float duration, const HM::Vector3& end, uint32_t joint = 0)
    {
        JointTrack track;
        track.Joint       = joint;
        track.Translation = { { 0.0f, duration }, { HM::Vector3(0.0f, 0.0f, 0.0f), end }, Interpolation::Linear };
        return AnimationClip{ name, duration, { track } };
    }

    // As TwoBoneStrip's Lift: the root up from 0 to 1 over 2 s.
    AnimationClip MakeLift() { return MakeTranslationClip("Lift", 2.0f, HM::Vector3(0.0f, 1.0f, 0.0f)); }

    // A walk cycle: 2 m along +Z each second.
    AnimationClip MakeWalk() { return MakeTranslationClip("Walk", 1.0f, HM::Vector3(0.0f, 0.0f, 2.0f)); }
}

TEST_CASE("ExtractRootMotion - a clip's movement, looping and clamped")
{
    const Skeleton      skeleton = MakeSkeleton();
    const AnimationClip lift     = MakeLift();
    const AnimationClip walk     = MakeWalk();

    CHECK(Near(ExtractRootMotion(skeleton, lift, 0, 0.0f, 2.0f, false).Translation, HM::Vector3(0.0f, 1.0f, 0.0f)));
    CHECK(Near(ExtractRootMotion(skeleton, lift, 0, 0.0f, 1.0f, false).Translation, HM::Vector3(0.0f, 0.5f, 0.0f)));

    SUBCASE("a looping clip adds a cycle per wrap")
    {
        CHECK(Near(ExtractRootMotion(skeleton, walk, 0, 0.0f, 2.5f, true).Translation, HM::Vector3(0.0f, 0.0f, 5.0f)));
        CHECK(Near(ExtractRootMotion(skeleton, walk, 0, 0.8f, 1.3f, true).Translation, HM::Vector3(0.0f, 0.0f, 1.0f)));
        CHECK(Near(ExtractRootMotion(skeleton, walk, 0, 3.2f, 3.4f, true).Translation, HM::Vector3(0.0f, 0.0f, 0.4f)));
    }
    SUBCASE("a clamped clip stops at its end")
    {
        CHECK(Near(ExtractRootMotion(skeleton, walk, 0, 0.5f, 3.0f, false).Translation, HM::Vector3(0.0f, 0.0f, 1.0f)));
        CHECK(Near(ExtractRootMotion(skeleton, walk, 0, 1.5f, 3.0f, false).Translation, HM::Vector3(0.0f, 0.0f, 0.0f)));
    }
    SUBCASE("nothing without a track of the joint, or for a joint out of range")
    {
        CHECK(Near(ExtractRootMotion(skeleton, walk, 1, 0.0f, 1.0f, true).Translation, HM::Vector3(0.0f, 0.0f, 0.0f)));
        CHECK(Near(ExtractRootMotion(skeleton, walk, 7, 0.0f, 1.0f, true).Translation, HM::Vector3(0.0f, 0.0f, 0.0f)));
    }
}

TEST_CASE("ExtractRootMotion - rotation, and a root under a turned parent")
{
    Skeleton   skeleton = MakeSkeleton();
    JointTrack turn;
    turn.Joint    = 0;
    turn.Rotation = { { 0.0f, 1.0f }, { HM::Quaternion::Identity(), AboutY(90.0f) }, Interpolation::Linear };
    const AnimationClip turning{ "Turn", 1.0f, { turn } };
    CHECK(SameRotation(ExtractRootMotion(skeleton, turning, 0, 0.0f, 1.0f, false).Rotation, AboutY(90.0f)));
    CHECK(SameRotation(ExtractRootMotion(skeleton, turning, 0, 0.0f, 2.0f, true).Rotation, AboutY(180.0f)));

    // Spine's parent turned a quarter about +Y: its movement along its own +Z is along model +X.
    skeleton.BindPose[0].Rotation = AboutY(90.0f);
    const AnimationClip spineWalk = MakeTranslationClip("SpineWalk", 1.0f, HM::Vector3(0.0f, 0.0f, 1.0f), 1);
    CHECK(Near(ExtractRootMotion(skeleton, spineWalk, 1, 0.0f, 1.0f, false).Translation, HM::Vector3(1.0f, 0.0f, 0.0f)));
}

TEST_CASE("AdvanceBlendStack - root motion of a crossfade and of a blend tree")
{
    const Skeleton                   skeleton = MakeSkeleton();
    const std::vector<AnimationClip> clips    = { MakeWalk(), MakeLift() };
    RootMotionDelta                  motion;

    SUBCASE("two entries at half weight each give the weighted delta")
    {
        BlendStack stack;
        PushClip(stack, 0, 1.0f, true, 0.0f);
        PushClip(stack, 1, 1.0f, false, 1.0f);
        AdvanceBlendStack(stack, 0.5f, 0.5f);
        REQUIRE(GetEffectiveWeight(stack, 0) == doctest::Approx(0.5f));
        AdvanceBlendStack(stack, 0.1f, 0.0f, skeleton, clips, 0, motion);
        // Walk 0.5 -> 0.6 s, 0.2 m along Z; Lift 0.5 -> 0.6 s, 0.05 m up; each at half weight.
        CHECK(Near(motion.Translation, HM::Vector3(0.0f, 0.025f, 0.1f)));
        CHECK(stack.Entries[1].Time == doctest::Approx(0.6f));
    }
    SUBCASE("a tree's children move over their own times, across a wrap")
    {
        BlendStack                           stack;
        const std::array<BlendEntryChild, 2> children = { BlendEntryChild{ 0, 0.5f, 1.0f, 1.0f },
                                                          BlendEntryChild{ 1, 0.5f, 2.0f, 1.0f } };
        PushBlendTree(stack, children, 1.0f, true, 0.0f);
        // The cycle is 1.5 s, so 0.15 s is a tenth: Walk 0.1 s, Lift 0.2 s.
        AdvanceBlendStack(stack, 0.15f, 0.0f, skeleton, clips, 0, motion);
        CHECK(Near(motion.Translation, HM::Vector3(0.0f, 0.05f, 0.1f)));
        // From 0.1 past the cycle's end to 0.2 of the next: Walk 0.1 s -> 1.2 s, 2.2 m, as a loop.
        AdvanceBlendStack(stack, 1.65f, 0.0f, skeleton, clips, 0, motion);
        CHECK(motion.Translation.z() == doctest::Approx(1.1f).epsilon(1e-3));
        CHECK(stack.Entries[0].Time == doctest::Approx(0.2f).epsilon(1e-3));
    }
}

TEST_CASE("RemoveRootMotion - only the masked axes go back to the bind pose")
{
    const std::vector<JointTransform> bind = { JointTransform{ HM::Vector3(0.0f, 0.5f, 0.0f) } };
    const HM::Quaternion              tilted =
        (AboutY(40.0f) * HM::Quaternion::FromAxisAngle(HM::Vector3(1.0f, 0.0f, 0.0f), 20.0f)).Normalize();
    std::vector<JointTransform> pose = { JointTransform{ HM::Vector3(1.0f, 2.0f, 3.0f), tilted } };

    SUBCASE("the default: horizontal translation and yaw")
    {
        RemoveRootMotion(pose, 0, bind);
        CHECK(Near(pose[0].Translation, HM::Vector3(0.0f, 2.0f, 0.0f)));
        // The twist about Y is gone; the tilt stays.
        CHECK(std::abs(pose[0].Rotation.y()) < EPSILON);
        CHECK(std::abs(pose[0].Rotation.x()) > 0.1f);
    }
    SUBCASE("an empty mask changes nothing, a full one all of the translation")
    {
        RemoveRootMotion(pose, 0, bind, RootMotionMask{ false, false, false, false });
        CHECK(Near(pose[0].Translation, HM::Vector3(1.0f, 2.0f, 3.0f)));
        CHECK(SameRotation(pose[0].Rotation, tilted));
        RemoveRootMotion(pose, 0, bind, RootMotionMask{ true, true, true, false });
        CHECK(Near(pose[0].Translation, HM::Vector3(0.0f, 0.5f, 0.0f)));
        RemoveRootMotion(pose, 3, bind); // out of range: nothing
    }
}
