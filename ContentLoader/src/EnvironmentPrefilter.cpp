#include "api/EnvironmentBake.hpp"

#include "EnvironmentSampling.hpp"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace ContentLoader
{
    namespace
    {
        using Sampling::PI;

        // One importance sample in the normal's frame, the same for every texel of a mip: the light
        // direction (the half vector reflected about the view, which is the normal), its cosine weight
        // and the source mip whose texels cover its solid angle.
        struct PrefilterSample
        {
            float Direction[3] = {};
            float Weight       = 0.0f;
            float Lod          = 0.0f;
        };

        std::vector<PrefilterSample> MakeSamples(float roughness, uint32_t sampleCount, uint32_t sourceSize, uint32_t sourceMips)
        {
            const float alpha = roughness * roughness;
            // A source texel's solid angle at mip 0, on average over the cube.
            const float texelSolidAngle = 4.0f * PI / (6.0f * static_cast<float>(sourceSize) * static_cast<float>(sourceSize));
            std::vector<PrefilterSample> samples;
            samples.reserve(sampleCount);
            for (uint32_t i = 0; i < sampleCount; ++i)
            {
                float x = 0.0f;
                float y = 0.0f;
                Sampling::Hammersley(i, sampleCount, x, y);
                float h[3];
                Sampling::SampleGgxHalfVector(x, y, alpha, h);

                // The view is the normal (0, 0, 1), so L = 2 (N.H) H - N and N.L = 2 (N.H)^2 - 1.
                const float nDotH = h[2];
                const float nDotL = 2.0f * nDotH * nDotH - 1.0f;
                if (nDotL <= 0.0f)
                    continue;

                PrefilterSample sample;
                sample.Direction[0] = 2.0f * nDotH * h[0];
                sample.Direction[1] = 2.0f * nDotH * h[1];
                sample.Direction[2] = nDotL;
                sample.Weight       = nDotL;

                // pdf(L) = D (N.H) / (4 V.H), and V.H = N.H here; a sample stands for 1 / (count pdf)
                // steradians, read from the mip whose texels are that large (one mip finer, Karis' bias).
                const float pdf          = Sampling::DistributionGgx(nDotH, alpha) * 0.25f;
                const float sampleAngle  = 1.0f / (static_cast<float>(sampleCount) * pdf + 1e-6f);
                const float lod          = 0.5f * std::log2(sampleAngle / texelSolidAngle) + 1.0f;
                sample.Lod               = std::clamp(lod, 0.0f, static_cast<float>(sourceMips - 1));
                samples.push_back(sample);
            }
            return samples;
        }

        // Bilinear inside one face of one mip, clamped to the face's edges.
        void SampleFace(const FloatCube& cube, uint32_t mip, CubeFace face, float u, float v, float out[3])
        {
            const uint32_t size   = cube.GetMipSize(mip);
            const float    last   = static_cast<float>(size - 1);
            const float    fx     = std::clamp(u * static_cast<float>(size) - 0.5f, 0.0f, last);
            const float    fy     = std::clamp(v * static_cast<float>(size) - 0.5f, 0.0f, last);
            const uint32_t x0     = static_cast<uint32_t>(fx);
            const uint32_t y0     = static_cast<uint32_t>(fy);
            const uint32_t x1     = std::min(x0 + 1, size - 1);
            const uint32_t y1     = std::min(y0 + 1, size - 1);
            const float    tx     = fx - static_cast<float>(x0);
            const float    ty     = fy - static_cast<float>(y0);
            const float*   texels = cube.Mips[mip][static_cast<uint32_t>(face)].data();
            for (uint32_t c = 0; c < 3; ++c)
            {
                const float a      = texels[(static_cast<size_t>(y0) * size + x0) * 4 + c];
                const float b      = texels[(static_cast<size_t>(y0) * size + x1) * 4 + c];
                const float d      = texels[(static_cast<size_t>(y1) * size + x0) * 4 + c];
                const float e      = texels[(static_cast<size_t>(y1) * size + x1) * 4 + c];
                const float top    = a + (b - a) * tx;
                const float bottom = d + (e - d) * tx;
                out[c]             = top + (bottom - top) * ty;
            }
        }

        // Trilinear: the two mips around lod, each bilinear.
        void SampleCube(const FloatCube& cube, const HM::Vector3& direction, float lod, float out[3])
        {
            const CubeTexel texel = DirectionToCubeTexel(direction);
            const uint32_t  mip0  = static_cast<uint32_t>(lod);
            const uint32_t  mip1  = std::min(mip0 + 1, static_cast<uint32_t>(cube.Mips.size() - 1));
            const float     t     = lod - static_cast<float>(mip0);
            SampleFace(cube, mip0, texel.Face, texel.U, texel.V, out);
            if (mip1 == mip0 || t <= 0.0f)
                return;
            float coarse[3];
            SampleFace(cube, mip1, texel.Face, texel.U, texel.V, coarse);
            for (uint32_t c = 0; c < 3; ++c)
                out[c] += (coarse[c] - out[c]) * t;
        }

        void PrefilterFace(const FloatCube& source, const std::vector<PrefilterSample>& samples, uint32_t size, CubeFace face,
                           std::vector<float>& target)
        {
            target.resize(static_cast<size_t>(size) * size * 4);
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const HM::Vector3 n = CubeTexelToDirection(face, (static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                                                               (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
                    // A frame around the normal: any tangent will do, the samples are symmetric about it.
                    const HM::Vector3 up = std::abs(n.z()) < 0.999f ? HM::Vector3(0.0f, 0.0f, 1.0f) : HM::Vector3(1.0f, 0.0f, 0.0f);
                    const HM::Vector3 t  = HM::Cross(up, n).Normalize();
                    const HM::Vector3 b  = HM::Cross(n, t);

                    float sum[3] = { 0.0f, 0.0f, 0.0f };
                    float weight = 0.0f;
                    for (const PrefilterSample& sample : samples)
                    {
                        const HM::Vector3 l = t * sample.Direction[0] + b * sample.Direction[1] + n * sample.Direction[2];
                        float             radiance[3];
                        SampleCube(source, l, sample.Lod, radiance);
                        for (uint32_t c = 0; c < 3; ++c)
                            sum[c] += radiance[c] * sample.Weight;
                        weight += sample.Weight;
                    }

                    float* texel = &target[(static_cast<size_t>(y) * size + x) * 4];
                    for (uint32_t c = 0; c < 3; ++c)
                        texel[c] = weight > 0.0f ? sum[c] / weight : 0.0f;
                    texel[3] = 1.0f;
                }
            }
        }
    }

    FloatCube PrefilterCube(const FloatCube& source, uint32_t sampleCount)
    {
        FloatCube result;
        result.FaceSize = source.FaceSize;
        if (source.Mips.empty() || source.FaceSize == 0)
            return result;

        const uint32_t mipCount = static_cast<uint32_t>(source.Mips.size());
        result.Mips.resize(mipCount);
        result.Mips[0] = source.Mips[0];
        for (uint32_t mip = 1; mip < mipCount; ++mip)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(mipCount - 1);
            const auto  samples   = MakeSamples(roughness, std::max(sampleCount, 1u), source.FaceSize, mipCount);
            const uint32_t size   = result.GetMipSize(mip);

            // One thread per face: the faces are independent and only read the source.
            std::vector<std::jthread> workers;
            workers.reserve(CUBE_FACE_COUNT);
            for (uint32_t f = 0; f < CUBE_FACE_COUNT; ++f)
            {
                workers.emplace_back([&, f] { PrefilterFace(source, samples, size, static_cast<CubeFace>(f), result.Mips[mip][f]); });
            }
        }
        return result;
    }
}
