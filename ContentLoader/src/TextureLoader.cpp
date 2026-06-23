#include "api/TextureLoader.hpp"
#include "api/CommonFunctions.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.hpp"

#include <stdexcept>

namespace ContentLoader
{
    TextureLoader::TextureLoader()
        : m_Data(nullptr)
        , m_Width(0)
        , m_Height(0)
        , m_Channels(0)
    {
    }

    TextureLoader::~TextureLoader()
    {
        if (m_Data)
        {
            stbi_image_free(m_Data);
        }
    }

    void TextureLoader::LoadTexture(const std::string& file,
                                     const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = "assets://" + file;
        const auto bytes = fileSystem.ReadFile(virtualPath);
        if (!bytes)
            throw std::runtime_error("Failed to read texture file: " + virtualPath);

        m_Data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes->data()),
            static_cast<int>(bytes->size()),
            &m_Width, &m_Height, &m_Channels, STBI_rgb_alpha);

        if (!m_Data)
            throw std::runtime_error("Failed to decode texture image: " + virtualPath);
    }

    int TextureLoader::GetWidth() const
    {
        return m_Width;
    }

    int TextureLoader::GetHeight() const
    {
        return m_Height;
    }

    int TextureLoader::GetChannels() const
    {
        return m_Channels;
    }

    void* TextureLoader::GetData() const
    {
        return m_Data;
    }
}
