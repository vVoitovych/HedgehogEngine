#pragma once

#include "AnimatorController.hpp"

#include <span>

// A blend tree state's weights and cycle length (AnimatorController.hpp's StateMotion), as free
// functions over plain data.
namespace HedgehogAnimation
{
    // Each child's weight for the tree's parameters at (x, y) (y unused by a 1D tree), written to
    // outWeights[0 .. Children.size()), the rest of outWeights zeroed; they sum to 1. A 1D tree lerps
    // between the two thresholds around x and holds its first or last child past them; a 2D tree
    // uses freeform cartesian gradient bands. A child at the sample point takes all the weight. A
    // clip state, a tree without children or outWeights too short to hold them gives all zeros.
    void ComputeBlendTreeWeights(const AnimatorState& state, float x, float y, std::span<float> outWeights);

    // The tree's cycle length in seconds: the weighted mean of each child's duration divided by its
    // speed (a speed not above 0 counted as 1); what StepStateMachine's clipDurations holds for a
    // tree. 0 when the weights sum to nothing or the spans differ in size.
    [[nodiscard]] float GetBlendTreeDuration(std::span<const float> weights, std::span<const float> childDurations,
                                             std::span<const float> childSpeeds);
}
