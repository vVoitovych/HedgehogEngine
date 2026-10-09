#include "api/EnvironmentBake.hpp"
#include "api/HalfFloat.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ContentLoader
{
    namespace
    {
        // Half-float RGBA of float RGBA, finite values past HALF_MAX clamped to it (a sun's texels can
        // pass it), so no texel becomes infinity.
        std::vector<uint16_t> ToHalf(const std::vector<float>& texels)
        {
            std::vector<uint16_t> result(texels.size());
            for (size_t i = 0; i < texels.size(); ++i)
            {
                const float value = std::isnan(texels[i]) ? 0.0f : std::clamp(texels[i], -HALF_MAX, HALF_MAX);
                result[i]         = FloatToHalf(value);
            }
            return result;
        }
    }

    std::optional<BakedEnvironment> BakeEnvironment(const std::string& file, const FS::FileSystemManager& fileSystem,
                                                    const EnvironmentBakeDesc& desc)
    {
        const std::string virtualPath = FS::ToAssetVirtualPath(file);
        if (desc.FaceSize == 0 || desc.SampleCount == 0)
        {
            LOGERROR("[Environment] ", virtualPath, " cannot be baked with a face size of ", desc.FaceSize, " and ",
                     desc.SampleCount, " samples.");
            return std::nullopt;
        }

        const auto start = std::chrono::steady_clock::now();
        const auto image = LoadHdrImage(virtualPath, fileSystem);
        if (!image)
            return std::nullopt;

        FloatCube cube = EquirectToCube(*image, desc.FaceSize);
        BuildCubeMips(cube);
        const FloatCube prefiltered = PrefilterCube(cube, desc.SampleCount);

        BakedEnvironment baked;
        baked.FaceSize = desc.FaceSize;
        baked.MipCount = static_cast<uint32_t>(prefiltered.Mips.size());
        baked.Sh       = ProjectShIrradiance(cube);
        baked.Mips.resize(prefiltered.Mips.size());
        for (size_t mip = 0; mip < prefiltered.Mips.size(); ++mip)
            for (uint32_t f = 0; f < CUBE_FACE_COUNT; ++f)
                baked.Mips[mip][f] = ToHalf(prefiltered.Mips[mip][f]);

        const auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        LOGINFO("[Environment] Baked ", virtualPath, " (", image->Width, "x", image->Height, ") into a ", desc.FaceSize,
                " cube of ", baked.MipCount, " mips in ", static_cast<int64_t>(std::lround(milliseconds)), " ms.");
        return baked;
    }
}
