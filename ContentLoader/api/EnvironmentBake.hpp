#pragma once

#include "ContentLoaderApi.hpp"
#include "LoadedEnvironment.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <optional>
#include <string>

// The CPU environment bake: an equirectangular Radiance .hdr turned into a world-space cubemap with
// its mip chain and its SH9 diffuse irradiance, the cube prefiltered for GGX specular, and the split-sum
// BRDF table. Pure functions over float buffers; BakeEnvironment is the one entry point the renderer calls.
//
// The equirect is Z-up: its top row is +Z, its bottom row -Z, and its horizontal centre looks
// along +X with +Y a quarter of the width to its left (u = 0.5 + atan2(-y, x) / 2pi, v = acos(z) / pi).
// A rotation of theta degrees turns the environment about +Z, counterclockwise seen from above: what
// the image shows along +X is seen along (cos theta, sin theta, 0).
namespace ContentLoader
{
    // Reads a Radiance .hdr (file under assets:// unless it names a mount) as linear RGB floats.
    // A file that cannot be read, or is not a Radiance image, logs one error and gives nullopt.
    CONTENT_LOADER_API std::optional<HdrImage> LoadHdrImage(const std::string& file,
                                                            const FS::FileSystemManager& fileSystem);

    // Where a direction (any non-zero length) meets the cube: its face and the position across it, u and
    // v in [0, 1], u to the right and v down as Vulkan addresses the face.
    struct CubeTexel
    {
        CubeFace Face = CubeFace::PositiveX;
        float    U    = 0.5f;
        float    V    = 0.5f;
    };

    CONTENT_LOADER_API CubeTexel DirectionToCubeTexel(const HM::Vector3& direction);

    // The unit direction through position (u, v) of face, the inverse of DirectionToCubeTexel.
    CONTENT_LOADER_API HM::Vector3 CubeTexelToDirection(CubeFace face, float u, float v);

    // The image's radiance along a direction, bilinearly filtered (wrapping horizontally, clamped at
    // the poles), the environment turned by rotationDegrees about +Z. Black for an empty image.
    CONTENT_LOADER_API HM::Vector3 SampleEquirect(const HdrImage& image, const HM::Vector3& direction,
                                                  float rotationDegrees = 0.0f);

    // A faceSize-square cube (mip 0 only, alpha 1) sampling the image at each texel's centre.
    CONTENT_LOADER_API FloatCube EquirectToCube(const HdrImage& image, uint32_t faceSize, float rotationDegrees = 0.0f);

    // Fills cube's mips below mip 0, each the 2x2 average of the one above (an odd edge's last texel
    // averaged with itself), down to 1x1. A constant cube stays exactly constant.
    CONTENT_LOADER_API void BuildCubeMips(FloatCube& cube);

    // The solid angle of texel (x, y) of a size-square cube face; the whole cube's sum to 4 pi.
    CONTENT_LOADER_API float CubeTexelSolidAngle(uint32_t x, uint32_t y, uint32_t size);

    // Projects the cube's mip 0 onto SH9 and convolves it with the cosine lobe (ShIrradiance).
    CONTENT_LOADER_API ShIrradiance ProjectShIrradiance(const FloatCube& cube);

    // The irradiance (over pi) from sh for a surface facing normal (unit).
    CONTENT_LOADER_API HM::Vector3 EvaluateShIrradiance(const ShIrradiance& sh, const HM::Vector3& normal);

    // The cube prefiltered for image-based specular: mip 0 copied, mip m (of the source's mip count)
    // the GGX-filtered radiance for roughness m / (mips - 1) (the material's roughness, squared as the
    // shader squares it), with the view along the normal. Each texel averages sampleCount importance
    // samples (a Hammersley set), each read from the source mip whose texels match its solid angle
    // (filtered importance sampling), so few samples stay smooth. source needs its mips (BuildCubeMips).
    // Faces are filtered on their own, so seams are not blended across faces.
    CONTENT_LOADER_API FloatCube PrefilterCube(const FloatCube& source, uint32_t sampleCount);

    // The split-sum scale and bias at one point: the GGX specular BRDF with height-correlated Smith
    // visibility and Schlick's Fresnel, integrated over the hemisphere for a view at N.V (clamped above
    // 0) and the material's roughness, with sampleCount importance samples.
    struct BrdfScaleBias
    {
        float Scale = 0.0f;
        float Bias  = 0.0f;
    };

    CONTENT_LOADER_API BrdfScaleBias IntegrateBrdf(float nDotV, float roughness, uint32_t sampleCount);

    // The whole size x size table (BrdfLut), in half floats.
    CONTENT_LOADER_API BrdfLut ComputeBrdfLut(uint32_t size, uint32_t sampleCount);

    // Loads file (LoadHdrImage), makes its cube (EquirectToCube, BuildCubeMips), its irradiance
    // (ProjectShIrradiance) and its prefiltered radiance (PrefilterCube) in half floats, values past
    // HALF_MAX clamped to it, and logs how long it took. A file that does not load, a face size or
    // sample count of 0 gives nullopt with one error.
    CONTENT_LOADER_API std::optional<BakedEnvironment> BakeEnvironment(const std::string& file,
                                                                       const FS::FileSystemManager& fileSystem,
                                                                       const EnvironmentBakeDesc& desc = {});
}
