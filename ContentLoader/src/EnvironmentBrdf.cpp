#include "api/EnvironmentBake.hpp"
#include "api/HalfFloat.hpp"

#include "EnvironmentSampling.hpp"

#include <algorithm>
#include <cmath>

namespace ContentLoader
{
    BrdfScaleBias IntegrateBrdf(float nDotV, float roughness, uint32_t sampleCount)
    {
        BrdfScaleBias result;
        if (sampleCount == 0)
            return result;

        // The normal is +Z and the view in the XZ plane; specular = F0 * Scale + Bias for Schlick's Fresnel.
        nDotV             = std::clamp(nDotV, 1e-4f, 1.0f);
        const float alpha = std::clamp(roughness, 0.0f, 1.0f) * std::clamp(roughness, 0.0f, 1.0f);
        const float v[3]  = { std::sqrt(1.0f - nDotV * nDotV), 0.0f, nDotV };
        for (uint32_t i = 0; i < sampleCount; ++i)
        {
            float x = 0.0f;
            float y = 0.0f;
            Sampling::Hammersley(i, sampleCount, x, y);
            float h[3];
            Sampling::SampleGgxHalfVector(x, y, alpha, h);

            const float vDotH = v[0] * h[0] + v[1] * h[1] + v[2] * h[2];
            const float nDotL = 2.0f * vDotH * h[2] - v[2]; // L = 2 (V.H) H - V
            const float nDotH = h[2];
            if (nDotL <= 0.0f || vDotH <= 0.0f || nDotH <= 0.0f)
                continue;

            // D Vis F N.L divided by the sample's pdf, D (N.H) / (4 V.H): D cancels.
            const float visibility = Sampling::VisibilitySmithGgxCorrelated(nDotV, nDotL, alpha) * 4.0f * nDotL * vDotH / nDotH;
            const float fresnel    = std::pow(1.0f - vDotH, 5.0f);
            result.Scale += (1.0f - fresnel) * visibility;
            result.Bias += fresnel * visibility;
        }
        result.Scale /= static_cast<float>(sampleCount);
        result.Bias /= static_cast<float>(sampleCount);
        return result;
    }

    BrdfLut ComputeBrdfLut(uint32_t size, uint32_t sampleCount)
    {
        BrdfLut lut;
        lut.Size = size;
        lut.Texels.resize(static_cast<size_t>(size) * size * 2);
        for (uint32_t y = 0; y < size; ++y)
        {
            const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
            for (uint32_t x = 0; x < size; ++x)
            {
                const float         nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                const BrdfScaleBias value = IntegrateBrdf(nDotV, roughness, sampleCount);
                const size_t        i     = (static_cast<size_t>(y) * size + x) * 2;
                lut.Texels[i]             = FloatToHalf(value.Scale);
                lut.Texels[i + 1]         = FloatToHalf(value.Bias);
            }
        }
        return lut;
    }
}
