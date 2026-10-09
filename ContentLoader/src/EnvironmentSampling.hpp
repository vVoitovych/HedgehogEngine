#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

// GGX importance sampling shared by the prefilter and the BRDF table, in a local frame whose +Z is the
// normal. Plain floats, since both loops run millions of times.
namespace ContentLoader::Sampling
{
    inline constexpr float PI = std::numbers::pi_v<float>;

    // The i-th of count points of the 2D Hammersley set: (i / count, the base-2 radical inverse of i).
    inline void Hammersley(uint32_t i, uint32_t count, float& x, float& y)
    {
        uint32_t bits = i;
        bits          = (bits << 16u) | (bits >> 16u);
        bits          = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits          = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits          = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits          = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        x             = static_cast<float>(i) / static_cast<float>(count);
        y             = static_cast<float>(bits) * 2.3283064365386963e-10f; // / 2^32
    }

    // A half vector drawn from GGX's distribution of normals (alpha = roughness squared) for the
    // point (x, y) in [0, 1)^2, in the local frame.
    inline void SampleGgxHalfVector(float x, float y, float alpha, float out[3])
    {
        const float phi      = 2.0f * PI * x;
        const float a2       = alpha * alpha;
        const float cosTheta = std::sqrt((1.0f - y) / (1.0f + (a2 - 1.0f) * y));
        const float sinTheta = std::sqrt(std::max(1.0f - cosTheta * cosTheta, 0.0f));
        out[0]               = sinTheta * std::cos(phi);
        out[1]               = sinTheta * std::sin(phi);
        out[2]               = cosTheta;
    }

    // GGX's normal distribution D at N.H.
    inline float DistributionGgx(float nDotH, float alpha)
    {
        const float a2    = alpha * alpha;
        const float denom = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
        return a2 / (PI * denom * denom);
    }

    // Height-correlated Smith visibility, as Pbr.glsl's VisibilitySmithGgxCorrelated.
    inline float VisibilitySmithGgxCorrelated(float nDotV, float nDotL, float alpha)
    {
        const float a2   = alpha * alpha;
        const float ggxV = nDotL * std::sqrt(nDotV * nDotV * (1.0f - a2) + a2);
        const float ggxL = nDotV * std::sqrt(nDotL * nDotL * (1.0f - a2) + a2);
        return 0.5f / std::max(ggxV + ggxL, 1e-5f);
    }
}
