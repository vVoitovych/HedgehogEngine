#include "doctest/doctest/doctest.h"

#include "HedgehogAnimation/api/BlendStack.hpp"
#include "HedgehogAnimation/api/BlendTree.hpp"
#include "HedgehogAnimation/api/Pose.hpp"

#include <array>
#include <cmath>
#include <numeric>
#include <vector>

using namespace HedgehogAnimation;

namespace
{
    constexpr float EPSILON = 1e-5f;

    AnimatorState Tree1D(std::initializer_list<float> thresholds)
    {
        AnimatorState state;
        state.Motion = StateMotion::BlendTree1D;
        for (const float threshold : thresholds)
            state.Children.push_back({ "Clip", threshold });
        return state;
    }

    AnimatorState Tree2D(std::initializer_list<std::array<float, 2>> positions)
    {
        AnimatorState state;
        state.Motion = StateMotion::BlendTree2D;
        for (const std::array<float, 2>& position : positions)
            state.Children.push_back({ "Clip", 0.0f, position });
        return state;
    }

    std::array<float, MAX_BLEND_TREE_CHILDREN> Weights(const AnimatorState& state, float x, float y = 0.0f)
    {
        std::array<float, MAX_BLEND_TREE_CHILDREN> weights{};
        ComputeBlendTreeWeights(state, x, y, weights);
        return weights;
    }

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

    // x runs from start to end over duration seconds, linearly.
    AnimationClip Slide(float start, float end, float duration)
    {
        JointTrack track;
        track.Joint       = 0;
        track.Translation = { { 0.0f, duration }, { HM::Vector3(start, 0.0f, 0.0f), HM::Vector3(end, 0.0f, 0.0f) },
                              Interpolation::Linear };
        return AnimationClip{ "Slide", duration, { track } };
    }

    float PoseX(const std::vector<JointTransform>& pose) { return pose[0].Translation.x(); }
}

TEST_CASE("BlendTree - 1D weights at, between and past the thresholds")
{
    const AnimatorState tree = Tree1D({ 0.0f, 1.0f, 3.0f });
    CHECK(Weights(tree, 0.0f)[0] == 1.0f);
    CHECK(Weights(tree, 1.0f)[1] == 1.0f);
    CHECK(Weights(tree, 3.0f)[2] == 1.0f);
    CHECK(Weights(tree, -5.0f)[0] == 1.0f);
    CHECK(Weights(tree, 9.0f)[2] == 1.0f);

    std::array<float, MAX_BLEND_TREE_CHILDREN> between = Weights(tree, 0.25f);
    CHECK(between[0] == doctest::Approx(0.75f));
    CHECK(between[1] == doctest::Approx(0.25f));
    CHECK(between[2] == 0.0f);
    between = Weights(tree, 2.5f);
    CHECK(between[0] == 0.0f);
    CHECK(between[1] == doctest::Approx(0.25f));
    CHECK(between[2] == doctest::Approx(0.75f));

    // One child takes everything; a clip state and too short an output give nothing.
    CHECK(Weights(Tree1D({ 2.0f }), 7.0f)[0] == 1.0f);
    AnimatorState clip = tree;
    clip.Motion        = StateMotion::Clip;
    CHECK(Weights(clip, 1.0f)[1] == 0.0f);
    std::array<float, 2> tooShort{ 9.0f, 9.0f };
    ComputeBlendTreeWeights(tree, 1.0f, 0.0f, tooShort);
    CHECK(tooShort[0] == 0.0f);
    CHECK(tooShort[1] == 0.0f);
}

TEST_CASE("BlendTree - 2D weights: each child alone at its position, summing to 1 and continuous everywhere")
{
    // A centre and four directions, as a walk and strafe tree is laid out.
    const AnimatorState tree = Tree2D({ { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { -1.0f, 0.0f } });
    for (size_t c = 0; c < tree.Children.size(); ++c)
    {
        CAPTURE(c);
        const auto weights = Weights(tree, tree.Children[c].Position[0], tree.Children[c].Position[1]);
        for (size_t other = 0; other < tree.Children.size(); ++other)
            CHECK(weights[other] == doctest::Approx(other == c ? 1.0f : 0.0f));
    }

    // A grid of samples 0.01 apart over and past the children: the weights sum to 1, and no weight
    // moves more than 0.05 between neighbours.
    constexpr int   STEPS = 300;
    constexpr float STEP  = 0.01f;
    std::vector<std::array<float, MAX_BLEND_TREE_CHILDREN>> row(STEPS + 1);
    std::vector<std::array<float, MAX_BLEND_TREE_CHILDREN>> previousRow;
    float worstJump = 0.0f;
    float worstSum  = 0.0f;
    for (int j = 0; j <= STEPS; ++j)
    {
        for (int i = 0; i <= STEPS; ++i)
        {
            row[i] = Weights(tree, -1.5f + STEP * i, -1.5f + STEP * j);
            worstSum = std::max(worstSum, std::abs(std::accumulate(row[i].begin(), row[i].end(), 0.0f) - 1.0f));
            for (size_t c = 0; c < tree.Children.size(); ++c)
            {
                if (i > 0)
                    worstJump = std::max(worstJump, std::abs(row[i][c] - row[i - 1][c]));
                if (!previousRow.empty())
                    worstJump = std::max(worstJump, std::abs(row[i][c] - previousRow[i][c]));
            }
        }
        previousRow = row;
    }
    CHECK(worstSum < EPSILON * 10.0f);
    CHECK(worstJump < 0.05f);

    // Halfway between the centre and the right, the two share it.
    const auto halfway = Weights(tree, 0.5f, 0.0f);
    CHECK(halfway[0] == doctest::Approx(0.5f));
    CHECK(halfway[2] == doctest::Approx(0.5f));
}

TEST_CASE("BlendTree - the cycle is the weighted mean of each child's duration over its speed")
{
    const std::array<float, 2> half      = { 0.5f, 0.5f };
    const std::array<float, 2> durations = { 1.0f, 2.0f };
    const std::array<float, 2> unit      = { 1.0f, 1.0f };
    CHECK(GetBlendTreeDuration(half, durations, unit) == doctest::Approx(1.5f));
    const std::array<float, 2> doubled = { 2.0f, 2.0f };
    CHECK(GetBlendTreeDuration(half, durations, doubled) == doctest::Approx(0.75f));
    const std::array<float, 2> none = { 0.0f, 0.0f };
    CHECK(GetBlendTreeDuration(none, durations, unit) == 0.0f);
    CHECK(GetBlendTreeDuration(half, std::array<float, 1>{ 1.0f }, unit) == 0.0f);
}

TEST_CASE("BlendStack - a tree entry plays its children in step, and loops on their weighted cycle")
{
    const Skeleton                   skeleton = MakeJoint();
    const std::vector<AnimationClip> clips    = { Slide(0.0f, 1.0f, 1.0f), Slide(10.0f, 12.0f, 2.0f) };
    std::vector<JointTransform>      scratch;
    std::vector<JointTransform>      pose;
    std::vector<JointTransform>      expected;
    std::vector<JointTransform>      other;

    const std::array<BlendEntryChild, 2> children = { BlendEntryChild{ 0, 0.0f, 1.0f }, BlendEntryChild{ 1, 0.0f, 2.0f } };
    BlendStack stack;
    PushBlendTree(stack, children, 1.0f, true, 0.0f);
    REQUIRE(stack.Count == 1u);
    REQUIRE(stack.Entries[0].ChildCount == 2u);

    // Half and half: a 1.5 s cycle, so 0.75 s is halfway through it and each child halfway through itself.
    SetEntryWeights(stack.Entries[0], std::array<float, 2>{ 0.5f, 0.5f });
    AdvanceBlendStack(stack, 0.75f, 0.75f);
    CHECK(stack.Entries[0].Time == doctest::Approx(0.5f));
    EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
    SamplePose(skeleton, clips[0], 0.5f, true, expected);
    SamplePose(skeleton, clips[1], 1.0f, true, other);
    BlendPoses(expected, other, 0.5f, expected);
    CHECK(PoseX(pose) == doctest::Approx(PoseX(expected)));
    AdvanceBlendStack(stack, 1.5f, 1.5f);
    CHECK(stack.Entries[0].Time == doctest::Approx(0.5f)); // a whole cycle later, wrapped

    // All weight on one child is that clip at that point of its own length.
    SetEntryWeights(stack.Entries[0], std::array<float, 2>{ 0.0f, 1.0f });
    EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
    SamplePose(skeleton, clips[1], 1.0f, true, expected);
    CHECK(PoseX(pose) == doctest::Approx(PoseX(expected)));
    CHECK(PoseX(pose) == doctest::Approx(11.0f));

    // Without a loop the tree holds its last pose.
    PushBlendTree(stack, children, 1.0f, false, 0.0f);
    SetEntryWeights(stack.Entries[0], std::array<float, 2>{ 1.0f, 0.0f });
    AdvanceBlendStack(stack, 5.0f, 5.0f);
    CHECK(stack.Entries[0].Time == 1.0f);
    EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
    CHECK(PoseX(pose) == doctest::Approx(1.0f));
}

TEST_CASE("BlendStack - a fade from a clip entry into a tree entry blends both")
{
    const Skeleton                   skeleton = MakeJoint();
    const std::vector<AnimationClip> clips    = { Slide(0.0f, 0.0f, 1.0f), Slide(10.0f, 10.0f, 1.0f),
                                                  Slide(20.0f, 20.0f, 1.0f) };
    std::vector<JointTransform>      scratch;
    std::vector<JointTransform>      pose;

    BlendStack stack;
    PushClip(stack, 0, 1.0f, true, 0.0f);
    const std::array<BlendEntryChild, 2> children = { BlendEntryChild{ 1, 0.0f, 1.0f }, BlendEntryChild{ 2, 0.0f, 1.0f } };
    PushBlendTree(stack, children, 1.0f, true, 1.0f);
    SetEntryWeights(stack.Entries[1], std::array<float, 2>{ 0.5f, 0.5f });
    REQUIRE(stack.Count == 2u);

    // Halfway through the fade: half the clip (0), half the tree (15).
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
    CHECK(PoseX(pose) == doctest::Approx(7.5f));
    AdvanceBlendStack(stack, 0.5f, 0.5f);
    REQUIRE(stack.Count == 1u);
    EvaluateBlendStack(stack, skeleton, clips, scratch, pose);
    CHECK(PoseX(pose) == doctest::Approx(15.0f));
}
