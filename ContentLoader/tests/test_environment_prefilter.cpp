#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/EnvironmentBake.hpp"
#include "ContentLoader/api/HalfFloat.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"
#include "test_hdr_files.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

using ContentLoader::FloatCube;
using ContentLoader::HdrImage;
using namespace HdrTest;

namespace
{
    // The largest red value of any texel of any face at a mip.
    float MipPeak(const FloatCube& cube, uint32_t mip)
    {
        float peak = 0.0f;
        for (const std::vector<float>& face : cube.Mips[mip])
            for (size_t i = 0; i < face.size(); i += 4)
                peak = std::max(peak, face[i]);
        return peak;
    }

    // The split-sum scale and bias by brute force: D Vis F N.L integrated over a fine grid of light
    // directions on the hemisphere, nothing importance sampled, so it checks IntegrateBrdf independently.
    ContentLoader::BrdfScaleBias ReferenceBrdf(float nDotV, float roughness)
    {
        const float  alpha = roughness * roughness;
        const float  a2    = alpha * alpha;
        const float  v[3]  = { std::sqrt(1.0f - nDotV * nDotV), 0.0f, nDotV };
        const int    thetaSteps = 1024;
        const int    phiSteps   = 256;
        const double dTheta     = 0.5 * std::numbers::pi / thetaSteps;
        const double dPhi       = 2.0 * std::numbers::pi / phiSteps;
        double       scale      = 0.0;
        double       bias       = 0.0;
        for (int t = 0; t < thetaSteps; ++t)
        {
            const double theta = (t + 0.5) * dTheta;
            for (int p = 0; p < phiSteps; ++p)
            {
                const double phi  = (p + 0.5) * dPhi;
                const double l[3] = { std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta) };
                double       h[3] = { l[0] + v[0], l[1] + v[1], l[2] + v[2] };
                const double len  = std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
                for (double& c : h)
                    c /= len;
                const double nDotL = l[2];
                const double nDotH = h[2];
                const double vDotH = h[0] * v[0] + h[1] * v[1] + h[2] * v[2];
                const double denom = nDotH * nDotH * (a2 - 1.0) + 1.0;
                const double d     = a2 / (std::numbers::pi * denom * denom);
                const double ggxV  = nDotL * std::sqrt(nDotV * nDotV * (1.0 - a2) + a2);
                const double ggxL  = nDotV * std::sqrt(nDotL * nDotL * (1.0 - a2) + a2);
                const double vis   = 0.5 / (ggxV + ggxL);
                const double fc    = std::pow(1.0 - vDotH, 5.0);
                const double f     = d * vis * nDotL * std::sin(theta) * dTheta * dPhi;
                scale += f * (1.0 - fc);
                bias += f * fc;
            }
        }
        return { static_cast<float>(scale), static_cast<float>(bias) };
    }
}

TEST_CASE("HalfFloat - exact values round-trip, others round to the nearest half")
{
    using ContentLoader::FloatToHalf;
    using ContentLoader::HalfToFloat;

    CHECK(FloatToHalf(0.0f) == 0x0000u);
    CHECK(FloatToHalf(-0.0f) == 0x8000u);
    CHECK(FloatToHalf(1.0f) == 0x3C00u);
    CHECK(FloatToHalf(-2.0f) == 0xC000u);
    CHECK(FloatToHalf(65504.0f) == 0x7BFFu);
    CHECK(FloatToHalf(0.5f + 1.0f / 4096.0f) == 0x3800u); // half an ulp past 0.5: a tie, to even
    CHECK(FloatToHalf(1e-3f) == 0x1419u);
    for (const float value : { 0.0f, 1.0f, -2.0f, 65504.0f, 0.333251953125f })
        CHECK(HalfToFloat(FloatToHalf(value)) == value);

    // Subnormals: the smallest half, 2^-24, and 3 * 2^-20; half of the smallest a tie rounding to 0.
    const float smallest = std::ldexp(1.0f, -24);
    CHECK(FloatToHalf(smallest) == 0x0001u);
    CHECK(HalfToFloat(0x0001u) == smallest);
    CHECK(HalfToFloat(FloatToHalf(3.0f * std::ldexp(1.0f, -20))) == 3.0f * std::ldexp(1.0f, -20));
    CHECK(FloatToHalf(smallest * 0.5f) == 0x0000u);
    CHECK(HalfToFloat(FloatToHalf(1e-3f)) == doctest::Approx(1e-3f).epsilon(1e-3));

    // Past the range infinity, and NaN stays NaN.
    CHECK(FloatToHalf(65520.0f) == 0x7C00u);
    CHECK(FloatToHalf(std::numeric_limits<float>::infinity()) == 0x7C00u);
    CHECK(std::isinf(HalfToFloat(0xFC00u)));
    CHECK(std::isnan(HalfToFloat(FloatToHalf(std::numeric_limits<float>::quiet_NaN()))));
}

TEST_CASE("PrefilterCube - mip 0 is the source, and a constant environment stays constant at every mip")
{
    FloatCube cube = ContentLoader::EquirectToCube(ConstantImage(32, 16, HM::Vector3(2.0f, 0.5f, 0.25f)), 16);
    ContentLoader::BuildCubeMips(cube);
    const FloatCube prefiltered = ContentLoader::PrefilterCube(cube, 32);

    REQUIRE(prefiltered.Mips.size() == 5);
    CHECK(prefiltered.FaceSize == 16u);
    CHECK(prefiltered.Mips[0] == cube.Mips[0]);
    for (uint32_t mip = 0; mip < prefiltered.Mips.size(); ++mip)
    {
        const uint32_t size = prefiltered.GetMipSize(mip);
        for (const std::vector<float>& face : prefiltered.Mips[mip])
        {
            REQUIRE(face.size() == static_cast<size_t>(size) * size * 4);
            for (size_t i = 0; i < face.size(); i += 4)
            {
                CAPTURE(mip);
                CHECK(face[i] == doctest::Approx(2.0f).epsilon(0.01));
                CHECK(face[i + 1] == doctest::Approx(0.5f).epsilon(0.01));
                CHECK(face[i + 2] == doctest::Approx(0.25f).epsilon(0.01));
                CHECK(face[i + 3] == 1.0f);
            }
        }
    }
}

TEST_CASE("PrefilterCube - one bright texel blurs more and more, its peak falling mip by mip")
{
    HdrImage image = ConstantImage(64, 32, HM::Vector3(0.0f, 0.0f, 0.0f));
    SetTexel(image, 32, 16, HM::Vector3(1000.0f, 1000.0f, 1000.0f)); // just below the horizon along +X
    FloatCube cube = ContentLoader::EquirectToCube(image, 32);
    ContentLoader::BuildCubeMips(cube);
    const FloatCube prefiltered = ContentLoader::PrefilterCube(cube, 32);

    REQUIRE(prefiltered.Mips.size() == 6);
    float previous = MipPeak(prefiltered, 0);
    CHECK(previous > 0.0f);
    for (uint32_t mip = 1; mip < prefiltered.Mips.size(); ++mip)
    {
        CAPTURE(mip);
        const float peak = MipPeak(prefiltered, mip);
        CHECK(peak > 0.0f);
        CHECK(peak < previous);
        previous = peak;
    }
}

TEST_CASE("IntegrateBrdf - matches a brute-force integration of the split sum")
{
    // A mirror seen head-on reflects everything: scale 1, bias 0.
    const auto mirror = ContentLoader::IntegrateBrdf(1.0f, 0.0f, 64);
    CHECK(mirror.Scale == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(mirror.Bias == doctest::Approx(0.0f));

    struct Point
    {
        float NDotV;
        float Roughness;
    };
    for (const Point point : { Point{ 0.5f, 0.25f }, Point{ 0.3f, 0.5f }, Point{ 0.8f, 0.5f }, Point{ 0.3f, 1.0f }, Point{ 0.8f, 1.0f } })
    {
        CAPTURE(point.NDotV);
        CAPTURE(point.Roughness);
        const auto sampled   = ContentLoader::IntegrateBrdf(point.NDotV, point.Roughness, 1024);
        const auto reference = ReferenceBrdf(point.NDotV, point.Roughness);
        CHECK(sampled.Scale == doctest::Approx(reference.Scale).epsilon(0.02));
        CHECK(sampled.Bias == doctest::Approx(reference.Bias).epsilon(0.03));
    }

    // Energy is never gained; seen head-on Fresnel adds almost nothing, and rougher surfaces lose more
    // to shadowing and masking.
    for (float nDotV = 0.05f; nDotV <= 1.0f; nDotV += 0.15f)
    {
        for (float roughness = 0.0f; roughness <= 1.0f; roughness += 0.2f)
        {
            const auto value = ContentLoader::IntegrateBrdf(nDotV, roughness, 256);
            CHECK(value.Scale + value.Bias <= 1.0f + 1e-3f);
        }
    }
    float previousScale = ContentLoader::IntegrateBrdf(1.0f, 0.2f, 256).Scale;
    for (const float roughness : { 0.4f, 0.6f, 0.8f, 1.0f })
    {
        CAPTURE(roughness);
        const auto value = ContentLoader::IntegrateBrdf(1.0f, roughness, 256);
        CHECK(value.Bias < 0.01f);
        CHECK(value.Scale < previousScale);
        previousScale = value.Scale;
    }
}

TEST_CASE("ComputeBrdfLut - N.V along x and roughness along y, as half floats")
{
    const ContentLoader::BrdfLut lut = ContentLoader::ComputeBrdfLut(32, 64);
    CHECK(lut.Size == 32u);
    REQUIRE(lut.Texels.size() == 32u * 32u * 2u);

    const auto at = [&](uint32_t x, uint32_t y)
    {
        const size_t i = (static_cast<size_t>(y) * 32 + x) * 2;
        return ContentLoader::BrdfScaleBias{ ContentLoader::HalfToFloat(lut.Texels[i]), ContentLoader::HalfToFloat(lut.Texels[i + 1]) };
    };
    // Smooth and head-on: near (1, 0).
    CHECK(at(31, 0).Scale == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(at(31, 0).Bias == doctest::Approx(0.0f).epsilon(0.01));
    // Each texel is IntegrateBrdf at its centre, to half precision.
    const auto expected = ContentLoader::IntegrateBrdf(12.5f / 32.0f, 20.5f / 32.0f, 64);
    CHECK(at(12, 20).Scale == doctest::Approx(expected.Scale).epsilon(0.002));
    CHECK(at(12, 20).Bias == doctest::Approx(expected.Bias).epsilon(0.002));
}

TEST_CASE("BakeEnvironment - a .hdr bakes to a full prefiltered half-float chain and its irradiance")
{
    // Bright above, dim below.
    HdrImage image = ConstantImage(64, 32, HM::Vector3(0.1f, 0.1f, 0.1f));
    for (uint32_t y = 0; y < 16; ++y)
        for (uint32_t x = 0; x < 64; ++x)
            SetTexel(image, x, y, HM::Vector3(2.0f, 2.0f, 2.0f));

    TempDir dir;
    WriteFile(dir.Path() / "sky.hdr", WriteRadiance(image));
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, dir.Path());

    LogCapture log;
    const auto baked = ContentLoader::BakeEnvironment("sky.hdr", fileSystem);
    REQUIRE(baked.has_value());
    CHECK(log.Lines("[ERROR").empty());
    const auto infos = log.Lines("[Environment] Baked");
    REQUIRE(infos.size() == 1);
    CHECK(infos[0].find("assets://sky.hdr") != std::string::npos);
    CHECK(infos[0].find("ms.") != std::string::npos);

    CHECK(baked->FaceSize == 256u);
    CHECK(baked->MipCount == 9u);
    REQUIRE(baked->Mips.size() == 9u);
    for (uint32_t mip = 0; mip < baked->MipCount; ++mip)
    {
        const size_t size = std::max(256u >> mip, 1u);
        for (const std::vector<uint16_t>& face : baked->Mips[mip])
            CHECK(face.size() == size * size * 4);
    }

    // The 1x1 mip of the +Z face sees the bright sky; the -Z face's the dim ground; alpha is 1.
    const auto& top    = baked->Mips[8][static_cast<uint32_t>(ContentLoader::CubeFace::PositiveZ)];
    const auto& bottom = baked->Mips[8][static_cast<uint32_t>(ContentLoader::CubeFace::NegativeZ)];
    CHECK(ContentLoader::HalfToFloat(top[0]) > ContentLoader::HalfToFloat(bottom[0]));
    CHECK(ContentLoader::HalfToFloat(top[3]) == 1.0f);

    // The irradiance: an upward face lit more than a downward one, within the sky's range.
    const HM::Vector3 up   = ContentLoader::EvaluateShIrradiance(baked->Sh, HM::Vector3(0.0f, 0.0f, 1.0f));
    const HM::Vector3 down = ContentLoader::EvaluateShIrradiance(baked->Sh, HM::Vector3(0.0f, 0.0f, -1.0f));
    CHECK(up.x() > down.x());
    CHECK(up.x() < 2.0f);
    CHECK(down.x() > 0.0f);
}

TEST_CASE("BakeEnvironment - a missing file or a bake of nothing logs one error and gives nothing")
{
    TempDir dir;
    WriteFile(dir.Path() / "sky.hdr", WriteRadiance(ConstantImage(8, 4, HM::Vector3(1.0f, 1.0f, 1.0f))));
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, dir.Path());

    struct Case
    {
        const char*                        File;
        ContentLoader::EnvironmentBakeDesc Desc;
    };
    for (const Case& c : { Case{ "missing.hdr", {} }, Case{ "sky.hdr", { 0, 32 } }, Case{ "sky.hdr", { 16, 0 } } })
    {
        CAPTURE(c.File);
        LogCapture log;
        CHECK_FALSE(ContentLoader::BakeEnvironment(c.File, fileSystem, c.Desc).has_value());
        const auto errors = log.Lines("[ERROR");
        REQUIRE(errors.size() == 1);
        CHECK(errors[0].find(std::string("assets://") + c.File) != std::string::npos);
        CHECK(log.Lines("Baked").empty());
    }
}
