#include "doctest/doctest/doctest.h"

#include "HedgehogRenderer/Graph/EnvironmentUniform.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

using Renderer::EnvironmentUniform;

TEST_CASE("EnvironmentUniform - std140 layout matches Pbr.glsl's EnvironmentData")
{
    CHECK(offsetof(EnvironmentUniform, Sh) == 0);
    CHECK(offsetof(EnvironmentUniform, Intensity) == 144);
    CHECK(offsetof(EnvironmentUniform, RotationCos) == 148);
    CHECK(offsetof(EnvironmentUniform, RotationSin) == 152);
    CHECK(offsetof(EnvironmentUniform, MaxMip) == 156);
    CHECK(sizeof(EnvironmentUniform) == 160);
}

TEST_CASE("MakeEnvironmentUniform - packs the SH times the intensity, the rotation and the last mip")
{
    ContentLoader::ShIrradiance sh;
    for (uint32_t i = 0; i < ContentLoader::SH_COEFFICIENT_COUNT; ++i)
        sh.Coefficients[i] = HM::Vector3(static_cast<float>(i), 0.5f, -1.0f);

    const EnvironmentUniform uniform = Renderer::MakeEnvironmentUniform(sh, 9, 2.0f, 90.0f);
    for (uint32_t i = 0; i < ContentLoader::SH_COEFFICIENT_COUNT; ++i)
    {
        CAPTURE(i);
        CHECK(uniform.Sh[i][0] == doctest::Approx(2.0f * static_cast<float>(i)));
        CHECK(uniform.Sh[i][1] == doctest::Approx(1.0f));
        CHECK(uniform.Sh[i][2] == doctest::Approx(-2.0f));
        CHECK(uniform.Sh[i][3] == 0.0f);
    }
    CHECK(uniform.Intensity == 2.0f);
    CHECK(uniform.RotationCos == doctest::Approx(0.0f));
    CHECK(uniform.RotationSin == doctest::Approx(1.0f));
    CHECK(uniform.MaxMip == 8.0f);

    const EnvironmentUniform turned = Renderer::MakeEnvironmentUniform(sh, 1, 1.0f, -180.0f);
    CHECK(turned.RotationCos == doctest::Approx(-1.0f));
    CHECK(turned.RotationSin == doctest::Approx(0.0f));
    CHECK(turned.MaxMip == 0.0f);
}

TEST_CASE("MakeEnvironmentUniform - bad values give no light and no turn, never NaN")
{
    ContentLoader::ShIrradiance sh;
    sh.Coefficients[0] = HM::Vector3(1.0f, 1.0f, 1.0f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    const EnvironmentUniform negative = Renderer::MakeEnvironmentUniform(sh, 0, -3.0f, nan);
    CHECK(negative.Intensity == 0.0f);
    CHECK(negative.Sh[0][0] == 0.0f);
    CHECK(negative.RotationCos == 1.0f);
    CHECK(negative.RotationSin == 0.0f);
    CHECK(negative.MaxMip == 0.0f);

    const EnvironmentUniform infinite = Renderer::MakeEnvironmentUniform(sh, 9, inf, inf);
    CHECK(infinite.Intensity == 0.0f);
    CHECK(infinite.RotationCos == 1.0f);

    const EnvironmentUniform none = Renderer::MakeNoEnvironmentUniform();
    CHECK(none.Intensity == 0.0f);
    CHECK(none.Sh[0][0] == 0.0f);
    CHECK(none.MaxMip == 0.0f);
}
