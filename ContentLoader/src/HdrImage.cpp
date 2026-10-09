#include "api/EnvironmentBake.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

// Declarations only: TextureLoader.cpp compiles stb_image's implementation.
#include "stb/stb_image.hpp"

#include <memory>

namespace ContentLoader
{
    std::optional<HdrImage> LoadHdrImage(const std::string& file, const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = FS::ToAssetVirtualPath(file);
        const auto        bytes       = fileSystem.ReadFile(virtualPath);
        if (!bytes)
        {
            LOGERROR("[Environment] ", virtualPath, " cannot be read.");
            return std::nullopt;
        }

        const auto* data = reinterpret_cast<const stbi_uc*>(bytes->data());
        const int   size = static_cast<int>(bytes->size());
        // stb would also load an 8-bit image as floats (decoding its gamma); only a Radiance image is HDR.
        if (!stbi_is_hdr_from_memory(data, size))
        {
            LOGERROR("[Environment] ", virtualPath, " is not a Radiance .hdr image.");
            return std::nullopt;
        }

        int width    = 0;
        int height   = 0;
        int channels = 0;
        const std::unique_ptr<float, void (*)(void*)> pixels(stbi_loadf_from_memory(data, size, &width, &height, &channels, 3),
                                                            &stbi_image_free);
        if (!pixels || width <= 0 || height <= 0)
        {
            LOGERROR("[Environment] ", virtualPath, " cannot be decoded: ", stbi_failure_reason() ? stbi_failure_reason() : "");
            return std::nullopt;
        }

        HdrImage image;
        image.Width  = static_cast<uint32_t>(width);
        image.Height = static_cast<uint32_t>(height);
        image.Pixels.assign(pixels.get(), pixels.get() + static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
        return image;
    }
}
