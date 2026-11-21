#include "HedgehogAnimation/api/BlendTree.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace HedgehogAnimation
{
    namespace
    {
        void Weights1D(const std::vector<BlendChild>& children, float x, std::span<float> out)
        {
            const size_t count = children.size();
            if (x <= children.front().Threshold || count == 1)
            {
                out[0] = 1.0f;
                return;
            }
            if (x >= children.back().Threshold)
            {
                out[count - 1] = 1.0f;
                return;
            }
            for (size_t i = 0; i + 1 < count; ++i)
            {
                const float low  = children[i].Threshold;
                const float high = children[i + 1].Threshold;
                if (x > high)
                    continue;
                const float t = high > low ? (x - low) / (high - low) : 1.0f;
                out[i]        = 1.0f - t;
                out[i + 1]    = t;
                return;
            }
        }

        // Freeform cartesian gradient bands: each child's weight is the least, over the other
        // children, of how far the point is from the band towards that child (1 at the child, 0 at
        // the other).
        void Weights2D(const std::vector<BlendChild>& children, float x, float y, std::span<float> out)
        {
            const size_t count = children.size();
            float        total = 0.0f;
            for (size_t i = 0; i < count; ++i)
            {
                const float px = children[i].Position[0];
                const float py = children[i].Position[1];
                float       weight = 1.0f;
                for (size_t j = 0; j < count && weight > 0.0f; ++j)
                {
                    if (j == i)
                        continue;
                    const float dx     = children[j].Position[0] - px;
                    const float dy     = children[j].Position[1] - py;
                    const float length = dx * dx + dy * dy;
                    if (length <= 0.0f)
                        continue;
                    const float along = ((x - px) * dx + (y - py) * dy) / length;
                    weight            = std::min(weight, std::clamp(1.0f - along, 0.0f, 1.0f));
                }
                out[i] = weight;
                total += weight;
            }
            if (total > 0.0f)
            {
                for (size_t i = 0; i < count; ++i)
                    out[i] /= total;
                return;
            }
            // No band holds the point: the nearest child takes it.
            size_t nearest  = 0;
            float  shortest = std::numeric_limits<float>::max();
            for (size_t i = 0; i < count; ++i)
            {
                const float dx       = x - children[i].Position[0];
                const float dy       = y - children[i].Position[1];
                const float distance = dx * dx + dy * dy;
                if (distance < shortest)
                {
                    shortest = distance;
                    nearest  = i;
                }
            }
            out[nearest] = 1.0f;
        }
    }

    void ComputeBlendTreeWeights(const AnimatorState& state, float x, float y, std::span<float> outWeights)
    {
        std::fill(outWeights.begin(), outWeights.end(), 0.0f);
        const std::vector<BlendChild>& children = state.Children;
        if (state.Motion == StateMotion::Clip || children.empty() || outWeights.size() < children.size())
            return;
        if (state.Motion == StateMotion::BlendTree1D)
            Weights1D(children, std::isfinite(x) ? x : 0.0f, outWeights);
        else
            Weights2D(children, std::isfinite(x) ? x : 0.0f, std::isfinite(y) ? y : 0.0f, outWeights);
    }

    float GetBlendTreeDuration(std::span<const float> weights, std::span<const float> childDurations,
                               std::span<const float> childSpeeds)
    {
        if (weights.size() != childDurations.size() || weights.size() != childSpeeds.size())
            return 0.0f;
        float total    = 0.0f;
        float duration = 0.0f;
        for (size_t i = 0; i < weights.size(); ++i)
        {
            const float speed = childSpeeds[i] > 0.0f ? childSpeeds[i] : 1.0f;
            total += weights[i];
            duration += weights[i] * childDurations[i] / speed;
        }
        return total > 0.0f ? duration / total : 0.0f;
    }
}
