#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/BlendStack.hpp"
#include "HedgehogAnimation/api/Pose.hpp"

#include <cmath>
#include <vector>

using namespace HedgehogAnimation;

namespace
{
    constexpr float EPSILON = 1e-4f;

    // One joint at the origin, so a pose is the joint's x translation.
    Skeleton MakeJoint()
    {
        Skeleton skeleton;
        skeleton.JointNames = { "Root" };
        skeleton.Parents    = { -1 };
        skeleton.BindPose   = { JointTransform{} };
        skeleton.InverseBind.push_back(HM::Matrix4x4::GetIdentity());
        return skeleton;
    }

    // x runs from start to end over 1 s, linearly.
    AnimationClip Slide(const char* name, float start, float end)
    {
        JointTrack track;
        track.Joint       = 0;
        track.Translation = { { 0.0f, 1.0f }, { HM::Vector3(start, 0.0f, 0.0f), HM::Vector3(end, 0.0f, 0.0f) },
                              Interpolation::Linear };
        return AnimationClip{ name, 1.0f, { track } };
    }

    // Clip 0 slides 0 -> 1, clip 1 holds 10, clip 2 holds 20, clip 3 holds 30, clip 4 holds 40.
    std::vector<AnimationClip> MakeClips()
    {
        return { Slide("Slide", 0.0f, 1.0f), Slide("Ten", 10.0f, 10.0f), Slide("Twenty", 20.0f, 20.0f),
                 Slide("Thirty", 30.0f, 30.0f), Slide("Forty", 40.0f, 40.0f) };
    }

    float PoseX(const BlendStack& stack)
    {
        static const Skeleton                    skeleton = MakeJoint();
        static const std::vector<AnimationClip> clips    = MakeClips();
        std::vector<JointTransform>              scratch;
        std::vector<JointTransform>              pose;
        EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
        return pose[0].Translation.x();
    }

    float WeightSum(const BlendStack& stack)
    {
        float sum = 0.0f;
        for (uint32_t i = 0; i < stack.Count; ++i)
            sum += GetEffectiveWeight(stack, i);
        return sum;
    }
}

TEST_CASE("BlendStack - one clip plays alone, and an empty stack gives the bind pose")
{
    BlendStack stack;
    CHECK(GetCurrentEntry(stack) == nullptr);
    CHECK(PoseX(stack) == 0.0f);

    PushClip(stack, 1, 1.0f, true, 0.0f);
    REQUIRE(stack.Count == 1u);
    CHECK(GetEffectiveWeight(stack, 0) == 1.0f);
    CHECK(std::abs(PoseX(stack) - 10.0f) < EPSILON);
    CHECK_FALSE(IsFading(stack));

    // With nothing playing, a fade has nothing to fade out of.
    ClearBlendStack(stack);
    PushClip(stack, 2, 1.0f, true, 0.5f);
    CHECK(stack.Count == 1u);
    CHECK(std::abs(PoseX(stack) - 20.0f) < EPSILON);
}

TEST_CASE("BlendStack - a fade moves the weight to the new clip over its duration, then leaves it alone")
{
    BlendStack stack;
    PushClip(stack, 1, 1.0f, true, 0.0f);
    PushClip(stack, 2, 1.0f, true, 1.0f);
    REQUIRE(stack.Count == 2u);
    CHECK(std::abs(PoseX(stack) - 10.0f) < EPSILON); // weight 0 at the switch

    AdvanceBlendStack(stack, 0.25f, 0.25f);
    CHECK(std::abs(PoseX(stack) - 12.5f) < EPSILON);
    CHECK(std::abs(WeightSum(stack) - 1.0f) < EPSILON);

    AdvanceBlendStack(stack, 0.5f, 0.5f);
    CHECK(std::abs(PoseX(stack) - 17.5f) < EPSILON);

    AdvanceBlendStack(stack, 0.25f, 0.25f);
    CHECK(stack.Count == 1u);
    CHECK_FALSE(IsFading(stack));
    CHECK(GetCurrentEntry(stack)->Clip == 2u);
    CHECK(std::abs(PoseX(stack) - 20.0f) < EPSILON);
}

TEST_CASE("BlendStack - a switch during a fade keeps the unfinished blend, so the pose does not jump")
{
    BlendStack stack;
    PushClip(stack, 1, 1.0f, true, 0.0f);
    PushClip(stack, 2, 1.0f, true, 1.0f);
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    const float before = PoseX(stack); // halfway: 15
    REQUIRE(std::abs(before - 15.0f) < EPSILON);

    PushClip(stack, 3, 1.0f, true, 1.0f);
    REQUIRE(stack.Count == 3u);
    CHECK(std::abs(PoseX(stack) - before) < EPSILON); // the same pose the moment it switches
    CHECK(std::abs(WeightSum(stack) - 1.0f) < EPSILON);

    // Halfway into the new fade: half the old blend (15), half the new clip (30).
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    CHECK(std::abs(PoseX(stack) - 22.5f) < EPSILON);
    CHECK(std::abs(WeightSum(stack) - 1.0f) < EPSILON);

    AdvanceBlendStack(stack, 0.5f, 0.5f);
    CHECK(stack.Count == 1u);
    CHECK(std::abs(PoseX(stack) - 30.0f) < EPSILON);
}

TEST_CASE("BlendStack - a fade of 0 cuts to the new clip")
{
    BlendStack stack;
    PushClip(stack, 1, 1.0f, true, 0.0f);
    PushClip(stack, 2, 1.0f, true, 1.0f);
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    PushClip(stack, 3, 1.0f, true, 0.0f);
    CHECK(stack.Count == 1u);
    CHECK(std::abs(PoseX(stack) - 30.0f) < EPSILON);
}

TEST_CASE("BlendStack - when full the oldest clip goes and the weights still sum to 1")
{
    BlendStack stack;
    PushClip(stack, 0, 1.0f, true, 0.0f);
    for (uint32_t clip = 1; clip <= 4; ++clip)
    {
        AdvanceBlendStack(stack, 0.1f, 0.1f);
        PushClip(stack, clip, 1.0f, true, 1.0f);
        CHECK(std::abs(WeightSum(stack) - 1.0f) < EPSILON);
    }
    CHECK(stack.Count == MAX_BLEND_ENTRIES);
    CHECK(stack.Entries[0].Clip == 1u); // the slide, pushed first, is gone
    CHECK(GetCurrentEntry(stack)->Clip == 4u);
}

TEST_CASE("BlendStack - each clip keeps its own loop and speed, and the fade runs on its own time")
{
    BlendStack stack;
    PushClip(stack, 0, 2.0f, false, 0.0f); // the slide at double speed, held at its end
    AdvanceBlendStack(stack, 0.75f, 0.75f);
    CHECK(std::abs(GetCurrentEntry(stack)->Time - 1.5f) < EPSILON);
    CHECK(std::abs(PoseX(stack) - 1.0f) < EPSILON); // clamped, not wrapped to 0.5

    stack.Entries[0].Loop = true;
    CHECK(std::abs(PoseX(stack) - 0.5f) < EPSILON); // wrapped

    // A clip pushed with another loop leaves the first one's as it was.
    PushClip(stack, 1, 1.0f, false, 1.0f);
    CHECK(stack.Entries[0].Loop);
    CHECK_FALSE(stack.Entries[1].Loop);

    // Clip time and fade time advance separately: a held clip still fades.
    AdvanceBlendStack(stack, 0.0f, 0.5f);
    CHECK(std::abs(GetCurrentEntry(stack)->Time) < EPSILON);
    CHECK(std::abs(GetEffectiveWeight(stack, 1) - 0.5f) < EPSILON);
}

TEST_CASE("BlendStack - a clip index past the mesh's clips is skipped")
{
    BlendStack stack;
    PushClip(stack, 99, 1.0f, true, 0.0f);
    CHECK(PoseX(stack) == 0.0f); // the bind pose
    PushClip(stack, 1, 1.0f, true, 1.0f);
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    CHECK(std::abs(PoseX(stack) - 10.0f) < EPSILON); // only the clip that exists
}
