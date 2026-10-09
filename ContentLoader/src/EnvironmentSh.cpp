#include "api/EnvironmentBake.hpp"

#include <array>
#include <cmath>
#include <numbers>

namespace ContentLoader
{
    namespace
    {
        constexpr float PI = std::numbers::pi_v<float>;

        // The real SH basis of bands 0 to 2 at a unit direction, in ShIrradiance's order.
        std::array<float, SH_COEFFICIENT_COUNT> ShBasis(const HM::Vector3& d)
        {
            const float x = d.x();
            const float y = d.y();
            const float z = d.z();
            return { 0.282095f,
                     0.488603f * y,
                     0.488603f * z,
                     0.488603f * x,
                     1.092548f * x * y,
                     1.092548f * y * z,
                     0.315392f * (3.0f * z * z - 1.0f),
                     1.092548f * x * z,
                     0.546274f * (x * x - y * y) };
        }

        // The band of each coefficient's cosine-lobe convolution, divided by pi: A0 = pi, A1 = 2pi/3,
        // A2 = pi/4 (Ramamoorthi and Hanrahan), so evaluating gives irradiance over pi.
        constexpr std::array<float, SH_COEFFICIENT_COUNT> BAND_SCALE = { 1.0f,
                                                                         2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f,
                                                                         0.25f, 0.25f, 0.25f, 0.25f, 0.25f };

        // The solid angle of the cube-face region from the face centre to (x, y), in [-1, 1] coordinates.
        float AreaElement(float x, float y)
        {
            return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0f));
        }
    }

    float CubeTexelSolidAngle(uint32_t x, uint32_t y, uint32_t size)
    {
        const float inverse = 1.0f / static_cast<float>(size);
        const float x0      = 2.0f * static_cast<float>(x) * inverse - 1.0f;
        const float y0      = 2.0f * static_cast<float>(y) * inverse - 1.0f;
        const float x1      = x0 + 2.0f * inverse;
        const float y1      = y0 + 2.0f * inverse;
        return AreaElement(x0, y0) - AreaElement(x0, y1) - AreaElement(x1, y0) + AreaElement(x1, y1);
    }

    ShIrradiance ProjectShIrradiance(const FloatCube& cube)
    {
        ShIrradiance sh;
        if (cube.Mips.empty() || cube.FaceSize == 0)
            return sh;

        // Radiance projected onto the basis, weighted by each texel's solid angle (sums in double:
        // a 256 face is 393 216 texels).
        std::array<std::array<double, 3>, SH_COEFFICIENT_COUNT> sums{};
        const uint32_t size = cube.FaceSize;
        for (uint32_t f = 0; f < CUBE_FACE_COUNT; ++f)
        {
            const std::vector<float>& face = cube.Mips[0][f];
            if (face.size() < static_cast<size_t>(size) * size * 4)
                continue;
            for (uint32_t y = 0; y < size; ++y)
            {
                for (uint32_t x = 0; x < size; ++x)
                {
                    const HM::Vector3 direction = CubeTexelToDirection(static_cast<CubeFace>(f),
                                                                       (static_cast<float>(x) + 0.5f) / static_cast<float>(size),
                                                                       (static_cast<float>(y) + 0.5f) / static_cast<float>(size));
                    const float  weight = CubeTexelSolidAngle(x, y, size);
                    const float* texel  = &face[(static_cast<size_t>(y) * size + x) * 4];
                    const auto   basis  = ShBasis(direction);
                    for (uint32_t i = 0; i < SH_COEFFICIENT_COUNT; ++i)
                        for (uint32_t c = 0; c < 3; ++c)
                            sums[i][c] += static_cast<double>(texel[c]) * basis[i] * weight;
                }
            }
        }

        for (uint32_t i = 0; i < SH_COEFFICIENT_COUNT; ++i)
        {
            sh.Coefficients[i] = HM::Vector3(static_cast<float>(sums[i][0]), static_cast<float>(sums[i][1]),
                                             static_cast<float>(sums[i][2]))
                               * BAND_SCALE[i];
        }
        return sh;
    }

    HM::Vector3 EvaluateShIrradiance(const ShIrradiance& sh, const HM::Vector3& normal)
    {
        const auto  basis = ShBasis(normal);
        HM::Vector3 result(0.0f, 0.0f, 0.0f);
        for (uint32_t i = 0; i < SH_COEFFICIENT_COUNT; ++i)
            result += sh.Coefficients[i] * basis[i];
        return result;
    }
}
