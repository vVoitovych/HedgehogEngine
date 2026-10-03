#pragma once

#include <memory>

namespace ContentLoader
{
    class TextureLoader;
}

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
}

namespace Editor
{
    // Uploads a decoded RGBA picture as a sampled sRGB texture, ready for ImGui to draw.
    [[nodiscard]] std::unique_ptr<RHI::IRHITexture> UploadIconTexture(const RHI::IRHIDevice& device,
                                                                      const ContentLoader::TextureLoader& image);
}
