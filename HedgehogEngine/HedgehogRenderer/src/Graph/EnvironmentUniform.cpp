#include "HedgehogRenderer/Graph/EnvironmentUniform.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Renderer
{
    namespace
    {
        float Finite(float value)
        {
            return std::isfinite(value) ? value : 0.0f;
        }
    }

    EnvironmentUniform MakeEnvironmentUniform(const ContentLoader::ShIrradiance& sh, uint32_t mipCount, float intensity,
                                              float rotationDegrees)
    {
        EnvironmentUniform uniform;
        uniform.Intensity = std::max(Finite(intensity), 0.0f);
        for (uint32_t i = 0; i < ContentLoader::SH_COEFFICIENT_COUNT; ++i)
        {
            uniform.Sh[i][0] = sh.Coefficients[i].x() * uniform.Intensity;
            uniform.Sh[i][1] = sh.Coefficients[i].y() * uniform.Intensity;
            uniform.Sh[i][2] = sh.Coefficients[i].z() * uniform.Intensity;
        }
        const float radians = Finite(rotationDegrees) * std::numbers::pi_v<float> / 180.0f;
        uniform.RotationCos = std::cos(radians);
        uniform.RotationSin = std::sin(radians);
        uniform.MaxMip      = mipCount > 0 ? static_cast<float>(mipCount - 1) : 0.0f;
        return uniform;
    }

    EnvironmentUniform MakeNoEnvironmentUniform()
    {
        return EnvironmentUniform{};
    }
}
