#pragma once

#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <cstdint>
#include <vector>

// Plain data of the CPU environment bake (EnvironmentBake.hpp): an equirectangular HDR image, the
// cubemap made from it, its diffuse irradiance as spherical harmonics, and what the bake hands the
// renderer: the prefiltered radiance cube and the BRDF table, in half floats.
namespace ContentLoader
{
    // A decoded Radiance .hdr: linear RGB floats, 3 per texel, row 0 the top row.
    struct HdrImage
    {
        uint32_t           Width  = 0;
        uint32_t           Height = 0;
        std::vector<float> Pixels;
    };

    // A cubemap's faces in Vulkan's layer order. Faces are in world space (the engine is Z-up, so +Z
    // is the sky), addressed as Vulkan samples a cube: a direction's largest axis picks the face.
    enum class CubeFace : uint32_t
    {
        PositiveX = 0,
        NegativeX = 1,
        PositiveY = 2,
        NegativeY = 3,
        PositiveZ = 4,
        NegativeZ = 5,
    };

    inline constexpr uint32_t CUBE_FACE_COUNT = 6;

    // A float cubemap with its mip chain: Mips[mip][face] holds RGBA floats, 4 per texel, row by row,
    // GetMipSize(mip) texels square.
    struct FloatCube
    {
        uint32_t                                                      FaceSize = 0;
        std::vector<std::array<std::vector<float>, CUBE_FACE_COUNT>> Mips;

        [[nodiscard]] uint32_t GetMipSize(uint32_t mip) const
        {
            const uint32_t size = mip < 32 ? FaceSize >> mip : 0u;
            return size > 0 ? size : 1u;
        }
    };

    inline constexpr uint32_t SH_COEFFICIENT_COUNT = 9;

    // Diffuse irradiance as the first nine real spherical harmonics (bands 0 to 2), already convolved
    // with the cosine lobe and divided by pi: EvaluateShIrradiance gives the radiance a white Lambert
    // surface facing a direction reflects, so a constant sky gives back its own radiance. Coefficient
    // order: Y00, Y1-1 (y), Y10 (z), Y11 (x), Y2-2 (xy), Y2-1 (yz), Y20 (3z^2 - 1), Y21 (xz), Y22 (x^2 - y^2).
    struct ShIrradiance
    {
        std::array<HM::Vector3, SH_COEFFICIENT_COUNT> Coefficients{};
    };

    // How BakeEnvironment works: the radiance cube's face size (mip 0; the chain goes down to 1x1) and
    // the GGX samples per texel of every prefiltered mip.
    struct EnvironmentBakeDesc
    {
        uint32_t FaceSize    = 256;
        uint32_t SampleCount = 32;
    };

    // A baked environment, ready to upload: the radiance cube prefiltered for GGX, mip m for roughness
    // m / (MipCount - 1) (mip 0 the environment itself, which the skybox shows), as half-float RGBA
    // (4 per texel, row by row, FaceSize >> m texels square, at least 1), Mips[mip][face] in Vulkan's
    // layer order; and its diffuse irradiance. Not rotated: the renderer turns directions instead.
    struct BakedEnvironment
    {
        uint32_t                                                         FaceSize = 0;
        uint32_t                                                         MipCount = 0;
        std::vector<std::array<std::vector<uint16_t>, CUBE_FACE_COUNT>> Mips;
        ShIrradiance                                                     Sh;
    };

    // The split-sum BRDF table: for N.V along x and roughness along y (texel centres, (i + 0.5) / Size),
    // the scale and bias applied to F0 (specular = prefiltered * (F0 * scale + bias)), as half-float RG
    // (2 per texel, row by row, row 0 roughness near 0).
    struct BrdfLut
    {
        uint32_t              Size = 0;
        std::vector<uint16_t> Texels;
    };
}
