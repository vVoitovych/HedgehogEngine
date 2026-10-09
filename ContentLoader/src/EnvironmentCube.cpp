#include "api/EnvironmentBake.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ContentLoader
{
    namespace
    {
        constexpr float PI = std::numbers::pi_v<float>;

        // The direction through face coordinates (s, t) in [-1, 1], Vulkan's cube addressing inverted.
        HM::Vector3 FaceDirection(CubeFace face, float s, float t)
        {
            switch (face)
            {
                case CubeFace::PositiveX: return HM::Vector3(1.0f, -t, -s);
                case CubeFace::NegativeX: return HM::Vector3(-1.0f, -t, s);
                case CubeFace::PositiveY: return HM::Vector3(s, 1.0f, t);
                case CubeFace::NegativeY: return HM::Vector3(s, -1.0f, -t);
                case CubeFace::PositiveZ: return HM::Vector3(s, -t, 1.0f);
                case CubeFace::NegativeZ: return HM::Vector3(-s, -t, -1.0f);
            }
            return HM::Vector3(1.0f, 0.0f, 0.0f);
        }

        HM::Vector3 Normalized(const HM::Vector3& v)
        {
            const float length = std::sqrt(v.x() * v.x() + v.y() * v.y() + v.z() * v.z());
            return length > 0.0f ? v / length : HM::Vector3(1.0f, 0.0f, 0.0f);
        }

        HM::Vector3 Texel(const HdrImage& image, uint32_t x, uint32_t y)
        {
            const size_t i = (static_cast<size_t>(y) * image.Width + x) * 3;
            return HM::Vector3(image.Pixels[i], image.Pixels[i + 1], image.Pixels[i + 2]);
        }
    }

    CubeTexel DirectionToCubeTexel(const HM::Vector3& direction)
    {
        const float x  = direction.x();
        const float y  = direction.y();
        const float z  = direction.z();
        const float ax = std::abs(x);
        const float ay = std::abs(y);
        const float az = std::abs(z);

        // Vulkan's cube addressing (the major axis picks the face; sc and tc span it).
        CubeTexel texel;
        float     sc = 0.0f;
        float     tc = 0.0f;
        float     ma = 1.0f;
        if (ax >= ay && ax >= az)
        {
            texel.Face = x >= 0.0f ? CubeFace::PositiveX : CubeFace::NegativeX;
            sc         = x >= 0.0f ? -z : z;
            tc         = -y;
            ma         = ax;
        }
        else if (ay >= az)
        {
            texel.Face = y >= 0.0f ? CubeFace::PositiveY : CubeFace::NegativeY;
            sc         = x;
            tc         = y >= 0.0f ? z : -z;
            ma         = ay;
        }
        else
        {
            texel.Face = z >= 0.0f ? CubeFace::PositiveZ : CubeFace::NegativeZ;
            sc         = z >= 0.0f ? x : -x;
            tc         = -y;
            ma         = az;
        }
        if (ma <= 0.0f)
            return texel;
        texel.U = std::clamp(0.5f * (sc / ma + 1.0f), 0.0f, 1.0f);
        texel.V = std::clamp(0.5f * (tc / ma + 1.0f), 0.0f, 1.0f);
        return texel;
    }

    HM::Vector3 CubeTexelToDirection(CubeFace face, float u, float v)
    {
        return Normalized(FaceDirection(face, 2.0f * u - 1.0f, 2.0f * v - 1.0f));
    }

    HM::Vector3 SampleEquirect(const HdrImage& image, const HM::Vector3& direction, float rotationDegrees)
    {
        if (image.Width == 0 || image.Height == 0 || image.Pixels.size() < static_cast<size_t>(image.Width) * image.Height * 3)
            return HM::Vector3(0.0f, 0.0f, 0.0f);

        // Turning the environment by theta shows, along a direction, what the image has theta before it.
        const HM::Vector3 d       = Normalized(direction);
        const float       azimuth = std::atan2(d.y(), d.x()) - rotationDegrees * PI / 180.0f;
        const float       u       = 0.5f - azimuth / (2.0f * PI);
        const float       v       = std::acos(std::clamp(d.z(), -1.0f, 1.0f)) / PI;

        // Bilinear between texel centres: wrapping across the seam, clamped at the poles.
        const float fx = u * static_cast<float>(image.Width) - 0.5f;
        const float fy = std::clamp(v * static_cast<float>(image.Height) - 0.5f, 0.0f, static_cast<float>(image.Height - 1));
        const float x0f = std::floor(fx);
        const float y0f = std::floor(fy);
        const float tx  = fx - x0f;
        const float ty  = fy - y0f;
        const auto  wrap = [&](float x)
        {
            const int64_t width = image.Width;
            return static_cast<uint32_t>(((static_cast<int64_t>(x) % width) + width) % width);
        };
        const uint32_t x0 = wrap(x0f);
        const uint32_t x1 = wrap(x0f + 1.0f);
        const uint32_t y0 = static_cast<uint32_t>(y0f);
        const uint32_t y1 = std::min(y0 + 1, image.Height - 1);

        // a + (b - a) t, so equal texels give exactly their value: a constant image stays constant.
        const auto lerp = [](const HM::Vector3& a, const HM::Vector3& b, float t) { return a + (b - a) * t; };
        const HM::Vector3 top    = lerp(Texel(image, x0, y0), Texel(image, x1, y0), tx);
        const HM::Vector3 bottom = lerp(Texel(image, x0, y1), Texel(image, x1, y1), tx);
        return lerp(top, bottom, ty);
    }

    FloatCube EquirectToCube(const HdrImage& image, uint32_t faceSize, float rotationDegrees)
    {
        FloatCube cube;
        cube.FaceSize = std::max(faceSize, 1u);
        cube.Mips.resize(1);
        const float size = static_cast<float>(cube.FaceSize);
        for (uint32_t f = 0; f < CUBE_FACE_COUNT; ++f)
        {
            std::vector<float>& face = cube.Mips[0][f];
            face.resize(static_cast<size_t>(cube.FaceSize) * cube.FaceSize * 4);
            for (uint32_t y = 0; y < cube.FaceSize; ++y)
            {
                for (uint32_t x = 0; x < cube.FaceSize; ++x)
                {
                    const HM::Vector3 direction = CubeTexelToDirection(static_cast<CubeFace>(f), (static_cast<float>(x) + 0.5f) / size,
                                                                       (static_cast<float>(y) + 0.5f) / size);
                    const HM::Vector3 radiance  = SampleEquirect(image, direction, rotationDegrees);
                    float* texel = &face[(static_cast<size_t>(y) * cube.FaceSize + x) * 4];
                    texel[0] = radiance.x();
                    texel[1] = radiance.y();
                    texel[2] = radiance.z();
                    texel[3] = 1.0f;
                }
            }
        }
        return cube;
    }

    void BuildCubeMips(FloatCube& cube)
    {
        if (cube.Mips.empty())
            return;
        cube.Mips.resize(1);
        for (uint32_t mip = 1; cube.GetMipSize(mip - 1) > 1; ++mip)
        {
            const uint32_t above = cube.GetMipSize(mip - 1);
            const uint32_t size  = cube.GetMipSize(mip);
            auto&          next  = cube.Mips.emplace_back(); // the mip above is read by index below, after this
            for (uint32_t f = 0; f < CUBE_FACE_COUNT; ++f)
            {
                const std::vector<float>& source = cube.Mips[mip - 1][f];
                std::vector<float>&       target = next[f];
                target.resize(static_cast<size_t>(size) * size * 4);
                for (uint32_t y = 0; y < size; ++y)
                {
                    const uint32_t y0 = std::min(2 * y, above - 1);
                    const uint32_t y1 = std::min(2 * y + 1, above - 1);
                    for (uint32_t x = 0; x < size; ++x)
                    {
                        const uint32_t x0 = std::min(2 * x, above - 1);
                        const uint32_t x1 = std::min(2 * x + 1, above - 1);
                        for (uint32_t c = 0; c < 4; ++c)
                        {
                            const auto at = [&](uint32_t sx, uint32_t sy) { return source[(static_cast<size_t>(sy) * above + sx) * 4 + c]; };
                            // Summed in pairs, so four equal values average back to exactly themselves.
                            target[(static_cast<size_t>(y) * size + x) * 4 + c] =
                                ((at(x0, y0) + at(x1, y0)) + (at(x0, y1) + at(x1, y1))) * 0.25f;
                        }
                    }
                }
            }
        }
    }
}
