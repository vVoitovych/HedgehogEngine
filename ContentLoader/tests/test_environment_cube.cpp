#include "doctest/doctest/doctest.h"

#include "ContentLoader/api/EnvironmentBake.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

using ContentLoader::CubeFace;
using ContentLoader::FloatCube;
using ContentLoader::HdrImage;

namespace
{
    constexpr float PI = std::numbers::pi_v<float>;

    // A width x height equirect where every texel is colour.
    HdrImage ConstantImage(uint32_t width, uint32_t height, const HM::Vector3& colour)
    {
        HdrImage image;
        image.Width  = width;
        image.Height = height;
        for (uint32_t i = 0; i < width * height; ++i)
            image.Pixels.insert(image.Pixels.end(), { colour.x(), colour.y(), colour.z() });
        return image;
    }

    void SetTexel(HdrImage& image, uint32_t x, uint32_t y, const HM::Vector3& colour)
    {
        const size_t i = (static_cast<size_t>(y) * image.Width + x) * 3;
        image.Pixels[i]     = colour.x();
        image.Pixels[i + 1] = colour.y();
        image.Pixels[i + 2] = colour.z();
    }

    HM::Vector3 CubeTexel(const FloatCube& cube, uint32_t mip, CubeFace face, uint32_t x, uint32_t y)
    {
        const uint32_t size = cube.GetMipSize(mip);
        const float*   t    = &cube.Mips[mip][static_cast<uint32_t>(face)][(static_cast<size_t>(y) * size + x) * 4];
        return HM::Vector3(t[0], t[1], t[2]);
    }

    // The sum of a face's red channel at a mip.
    float FaceSum(const FloatCube& cube, uint32_t mip, CubeFace face)
    {
        float sum = 0.0f;
        const std::vector<float>& texels = cube.Mips[mip][static_cast<uint32_t>(face)];
        for (size_t i = 0; i < texels.size(); i += 4)
            sum += texels[i];
        return sum;
    }

    // One RGBE texel of a Radiance file: a shared exponent and three 8-bit mantissas.
    void AppendRgbe(std::string& out, float r, float g, float b)
    {
        const float largest = std::max({ r, g, b });
        if (largest < 1e-32f)
        {
            out.append(4, '\0');
            return;
        }
        int         exponent = 0;
        const float mantissa = std::frexp(largest, &exponent) * 256.0f / largest;
        out.push_back(static_cast<char>(static_cast<unsigned char>(r * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(g * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(b * mantissa)));
        out.push_back(static_cast<char>(static_cast<unsigned char>(exponent + 128)));
    }

    // A flat (not run-length encoded) Radiance .hdr of the image.
    std::string WriteRadiance(const HdrImage& image)
    {
        std::string out = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y " + std::to_string(image.Height) + " +X "
                        + std::to_string(image.Width) + "\n";
        for (size_t i = 0; i < image.Pixels.size(); i += 3)
            AppendRgbe(out, image.Pixels[i], image.Pixels[i + 1], image.Pixels[i + 2]);
        return out;
    }

    void WriteFile(const std::filesystem::path& path, const std::string& bytes)
    {
        std::ofstream file(path, std::ios::binary);
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    void MountAssets(FS::FileSystemManager& manager, const std::filesystem::path& dir)
    {
        auto fs = std::make_unique<FS::FileSystem>();
        fs->RegisterPath("assets://", dir);
        REQUIRE(manager.Register(std::move(fs)));
    }

    bool Near(const HM::Vector3& a, const HM::Vector3& b, float epsilon)
    {
        return std::abs(a.x() - b.x()) <= epsilon && std::abs(a.y() - b.y()) <= epsilon && std::abs(a.z() - b.z()) <= epsilon;
    }
}

TEST_CASE("Environment cube - directions map to Vulkan's cube faces and back, at centres and corners")
{
    struct Axis
    {
        HM::Vector3 Direction;
        CubeFace    Face;
    };
    const Axis axes[] = {
        { HM::Vector3(1.0f, 0.0f, 0.0f), CubeFace::PositiveX },  { HM::Vector3(-1.0f, 0.0f, 0.0f), CubeFace::NegativeX },
        { HM::Vector3(0.0f, 1.0f, 0.0f), CubeFace::PositiveY },  { HM::Vector3(0.0f, -1.0f, 0.0f), CubeFace::NegativeY },
        { HM::Vector3(0.0f, 0.0f, 1.0f), CubeFace::PositiveZ },  { HM::Vector3(0.0f, 0.0f, -1.0f), CubeFace::NegativeZ },
    };
    for (const Axis& axis : axes)
    {
        const ContentLoader::CubeTexel texel = ContentLoader::DirectionToCubeTexel(axis.Direction * 3.0f);
        CHECK(texel.Face == axis.Face);
        CHECK(texel.U == doctest::Approx(0.5f));
        CHECK(texel.V == doctest::Approx(0.5f));
        CHECK(Near(ContentLoader::CubeTexelToDirection(axis.Face, 0.5f, 0.5f), axis.Direction, 1e-6f));
    }

    // Vulkan's +X face: u runs along -Z, v along -Y; +Z's: u along +X, v along -Y.
    CHECK(Near(ContentLoader::CubeTexelToDirection(CubeFace::PositiveX, 1.0f, 0.5f), HM::Vector3(1.0f, 0.0f, -1.0f) / std::sqrt(2.0f), 1e-6f));
    CHECK(Near(ContentLoader::CubeTexelToDirection(CubeFace::PositiveZ, 0.5f, 0.0f), HM::Vector3(0.0f, 1.0f, 1.0f) / std::sqrt(2.0f), 1e-6f));

    // Every face's texel centres and near-corners survive the round trip.
    for (uint32_t f = 0; f < ContentLoader::CUBE_FACE_COUNT; ++f)
    {
        for (const float u : { 0.01f, 0.25f, 0.5f, 0.9f, 0.99f })
        {
            for (const float v : { 0.01f, 0.4f, 0.5f, 0.75f, 0.99f })
            {
                CAPTURE(f);
                CAPTURE(u);
                CAPTURE(v);
                const HM::Vector3              direction = ContentLoader::CubeTexelToDirection(static_cast<CubeFace>(f), u, v);
                const ContentLoader::CubeTexel back      = ContentLoader::DirectionToCubeTexel(direction);
                CHECK(back.Face == static_cast<CubeFace>(f));
                CHECK(back.U == doctest::Approx(u).epsilon(1e-4));
                CHECK(back.V == doctest::Approx(v).epsilon(1e-4));
            }
        }
    }
}

TEST_CASE("Environment cube - a constant image gives an exactly constant cube at every mip")
{
    const HM::Vector3 colour(0.1f, 2.5f, 7.3f);
    FloatCube         cube = ContentLoader::EquirectToCube(ConstantImage(64, 32, colour), 32);
    ContentLoader::BuildCubeMips(cube);
    REQUIRE(cube.Mips.size() == 6); // 32, 16, 8, 4, 2, 1
    CHECK(cube.GetMipSize(5) == 1u);
    for (uint32_t mip = 0; mip < cube.Mips.size(); ++mip)
    {
        const uint32_t size = cube.GetMipSize(mip);
        for (uint32_t f = 0; f < ContentLoader::CUBE_FACE_COUNT; ++f)
        {
            CAPTURE(mip);
            CAPTURE(f);
            const std::vector<float>& texels = cube.Mips[mip][f];
            REQUIRE(texels.size() == static_cast<size_t>(size) * size * 4);
            for (size_t i = 0; i < texels.size(); i += 4)
            {
                REQUIRE(texels[i] == colour.x());
                REQUIRE(texels[i + 1] == colour.y());
                REQUIRE(texels[i + 2] == colour.z());
                REQUIRE(texels[i + 3] == 1.0f);
            }
        }
    }
}

TEST_CASE("Environment cube - the equirect's top row is +Z, its centre +X, and a quarter to the left +Y")
{
    // Top rows bright red, bottom rows blue; the middle column of the middle rows green.
    HdrImage image = ConstantImage(64, 32, HM::Vector3(0.0f, 0.0f, 0.0f));
    for (uint32_t x = 0; x < 64; ++x)
    {
        for (uint32_t y = 0; y < 3; ++y)
        {
            SetTexel(image, x, y, HM::Vector3(5.0f, 0.0f, 0.0f));
            SetTexel(image, x, 31 - y, HM::Vector3(0.0f, 0.0f, 5.0f));
        }
    }
    for (uint32_t y = 14; y < 18; ++y)
        for (uint32_t x = 30; x < 34; ++x)
            SetTexel(image, x, y, HM::Vector3(0.0f, 5.0f, 0.0f));
    for (uint32_t y = 14; y < 18; ++y)
        for (uint32_t x = 14; x < 18; ++x) // a quarter of the width left of centre
            SetTexel(image, x, y, HM::Vector3(1.0f, 1.0f, 1.0f));

    CHECK(ContentLoader::SampleEquirect(image, HM::Vector3(0.0f, 0.0f, 1.0f)).x() == doctest::Approx(5.0f));
    CHECK(ContentLoader::SampleEquirect(image, HM::Vector3(0.0f, 0.0f, -1.0f)).z() == doctest::Approx(5.0f));
    CHECK(ContentLoader::SampleEquirect(image, HM::Vector3(1.0f, 0.0f, 0.0f)).y() == doctest::Approx(5.0f));
    CHECK(Near(ContentLoader::SampleEquirect(image, HM::Vector3(0.0f, 1.0f, 0.0f)), HM::Vector3(1.0f, 1.0f, 1.0f), 1e-5f));

    FloatCube cube = ContentLoader::EquirectToCube(image, 16);
    CHECK(CubeTexel(cube, 0, CubeFace::PositiveZ, 8, 8).x() == doctest::Approx(5.0f));
    CHECK(CubeTexel(cube, 0, CubeFace::NegativeZ, 8, 8).z() == doctest::Approx(5.0f));
    CHECK(CubeTexel(cube, 0, CubeFace::PositiveX, 8, 8).y() > 1.0f);
    CHECK(CubeTexel(cube, 0, CubeFace::PositiveY, 8, 8).x() > 0.2f);
}

TEST_CASE("Environment cube - a rotation of 90 degrees about +Z moves a feature from the +X face to the +Y face")
{
    HdrImage image = ConstantImage(64, 32, HM::Vector3(0.0f, 0.0f, 0.0f));
    for (uint32_t y = 12; y < 20; ++y)
        for (uint32_t x = 28; x < 36; ++x)
            SetTexel(image, x, y, HM::Vector3(10.0f, 10.0f, 10.0f)); // around +X

    const FloatCube still   = ContentLoader::EquirectToCube(image, 16);
    const FloatCube rotated = ContentLoader::EquirectToCube(image, 16, 90.0f);
    CHECK(FaceSum(still, 0, CubeFace::PositiveX) > 0.0f);
    CHECK(FaceSum(still, 0, CubeFace::PositiveY) == 0.0f);
    CHECK(FaceSum(rotated, 0, CubeFace::PositiveX) == 0.0f);
    CHECK(FaceSum(rotated, 0, CubeFace::PositiveY) == doctest::Approx(FaceSum(still, 0, CubeFace::PositiveX)).epsilon(0.05));
    // The whole turn: -90 degrees puts it on -Y, 180 on -X.
    CHECK(FaceSum(ContentLoader::EquirectToCube(image, 16, -90.0f), 0, CubeFace::NegativeY) > 0.0f);
    CHECK(FaceSum(ContentLoader::EquirectToCube(image, 16, 180.0f), 0, CubeFace::NegativeX) > 0.0f);
}

TEST_CASE("Environment cube - mips average their 2x2 texels, and an odd edge averages with itself")
{
    FloatCube cube;
    cube.FaceSize = 2;
    cube.Mips.resize(1);
    for (uint32_t f = 0; f < ContentLoader::CUBE_FACE_COUNT; ++f)
        cube.Mips[0][f] = { 1, 0, 0, 1,  3, 0, 0, 1,  5, 0, 0, 1,  7, 0, 0, 1 };
    ContentLoader::BuildCubeMips(cube);
    REQUIRE(cube.Mips.size() == 2);
    CHECK(CubeTexel(cube, 1, CubeFace::NegativeZ, 0, 0).x() == 4.0f);

    FloatCube odd;
    odd.FaceSize = 3;
    odd.Mips.resize(1);
    for (uint32_t f = 0; f < ContentLoader::CUBE_FACE_COUNT; ++f)
        odd.Mips[0][f] = std::vector<float>(3 * 3 * 4, 2.0f);
    ContentLoader::BuildCubeMips(odd);
    REQUIRE(odd.Mips.size() == 2); // 3, then 1
    CHECK(CubeTexel(odd, 1, CubeFace::PositiveX, 0, 0).x() == 2.0f);
}

TEST_CASE("Environment SH - texel solid angles cover the sphere, and a constant sky's irradiance is its radiance")
{
    for (const uint32_t size : { 1u, 4u, 32u })
    {
        double total = 0.0;
        for (uint32_t y = 0; y < size; ++y)
            for (uint32_t x = 0; x < size; ++x)
                total += ContentLoader::CubeTexelSolidAngle(x, y, size);
        CHECK(total * 6.0 == doctest::Approx(4.0 * PI).epsilon(1e-5));
    }

    const HM::Vector3 colour(0.5f, 1.0f, 3.0f);
    const FloatCube   cube = ContentLoader::EquirectToCube(ConstantImage(64, 32, colour), 32);
    const auto        sh   = ContentLoader::ProjectShIrradiance(cube);
    for (const HM::Vector3& normal : { HM::Vector3(0.0f, 0.0f, 1.0f), HM::Vector3(0.0f, 0.0f, -1.0f), HM::Vector3(1.0f, 0.0f, 0.0f),
                                       HM::Vector3(0.0f, -0.6f, 0.8f) })
    {
        const HM::Vector3 irradiance = ContentLoader::EvaluateShIrradiance(sh, normal);
        CHECK(irradiance.x() == doctest::Approx(colour.x()).epsilon(0.01));
        CHECK(irradiance.y() == doctest::Approx(colour.y()).epsilon(0.01));
        CHECK(irradiance.z() == doctest::Approx(colour.z()).epsilon(0.01));
    }
    // Only the constant band is lit.
    for (uint32_t i = 1; i < ContentLoader::SH_COEFFICIENT_COUNT; ++i)
        CHECK(std::abs(sh.Coefficients[i].y()) < 1e-3f);
}

TEST_CASE("Environment SH - a sky bright above and dark below lights an upward face more than a downward one")
{
    HdrImage image = ConstantImage(64, 32, HM::Vector3(0.0f, 0.0f, 0.0f));
    for (uint32_t y = 0; y < 16; ++y)
        for (uint32_t x = 0; x < 64; ++x)
            SetTexel(image, x, y, HM::Vector3(2.0f, 2.0f, 2.0f));
    const auto sh = ContentLoader::ProjectShIrradiance(ContentLoader::EquirectToCube(image, 32));

    const float up   = ContentLoader::EvaluateShIrradiance(sh, HM::Vector3(0.0f, 0.0f, 1.0f)).x();
    const float side = ContentLoader::EvaluateShIrradiance(sh, HM::Vector3(1.0f, 0.0f, 0.0f)).x();
    const float down = ContentLoader::EvaluateShIrradiance(sh, HM::Vector3(0.0f, 0.0f, -1.0f)).x();
    // A face looking at a uniformly bright hemisphere sees all of it, one looking away none; the
    // horizon sees half. SH9 rings a little, so within 10 %.
    CHECK(up == doctest::Approx(2.0f).epsilon(0.1));
    CHECK(side == doctest::Approx(1.0f).epsilon(0.1));
    CHECK(std::abs(down) < 0.2f);
    CHECK(up > side);
    CHECK(side > down);
}

TEST_CASE("LoadHdrImage - a Radiance file reads as linear floats, values above 1 included")
{
    HdrImage written = ConstantImage(16, 8, HM::Vector3(0.25f, 1.0f, 4.0f));
    SetTexel(written, 3, 2, HM::Vector3(100.0f, 0.5f, 0.0f));

    TempDir dir;
    WriteFile(dir.Path() / "sky.hdr", WriteRadiance(written));
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, dir.Path());

    LogCapture log;
    const auto image = ContentLoader::LoadHdrImage("sky.hdr", fileSystem);
    REQUIRE(image.has_value());
    CHECK(log.Lines("[ERROR").empty());
    CHECK(image->Width == 16u);
    CHECK(image->Height == 8u);
    REQUIRE(image->Pixels.size() == 16u * 8u * 3u);
    CHECK(image->Pixels[0] == doctest::Approx(0.25f).epsilon(0.01));
    CHECK(image->Pixels[1] == doctest::Approx(1.0f).epsilon(0.01));
    CHECK(image->Pixels[2] == doctest::Approx(4.0f).epsilon(0.01));
    const size_t bright = (2 * 16 + 3) * 3;
    CHECK(image->Pixels[bright] == doctest::Approx(100.0f).epsilon(0.01));

    // The same file through assets:// named in full, with a backslash.
    CHECK(ContentLoader::LoadHdrImage("assets://sky.hdr", fileSystem).has_value());
}

TEST_CASE("LoadHdrImage - a missing file and one that is not a Radiance image each log one error")
{
    TempDir dir;
    WriteFile(dir.Path() / "not_hdr.hdr", "this is not an image");
    // An 8-bit image stb would happily decode as floats: refused, since it is not HDR.
    WriteFile(dir.Path() / "ldr.hdr", std::string("P6\n1 1\n255\n") + std::string("\x10\x20\x30", 3));
    FS::FileSystemManager fileSystem;
    MountAssets(fileSystem, dir.Path());

    for (const char* file : { "missing.hdr", "not_hdr.hdr", "ldr.hdr" })
    {
        CAPTURE(file);
        LogCapture log;
        CHECK_FALSE(ContentLoader::LoadHdrImage(file, fileSystem).has_value());
        const auto errors = log.Lines("[ERROR");
        REQUIRE(errors.size() == 1);
        CHECK(errors[0].find(std::string("assets://") + file) != std::string::npos);
    }
}
