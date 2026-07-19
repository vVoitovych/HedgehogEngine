#include "api/TextureLoader.hpp"

#include "Logger/api/Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.hpp"

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

    bool TextureLoader::LoadTexture(const std::string& file,
                                     const FS::FileSystemManager& fileSystem)
    {
        const std::string virtualPath = "assets://" + file;
        const auto bytes = fileSystem.ReadFile(virtualPath);
        if (!bytes)
        {
            LOGERROR("Failed to read texture file: ", virtualPath);
            return false;
        }

        m_Data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(bytes->data()),
            static_cast<int>(bytes->size()),
            &m_Width, &m_Height, &m_Channels, STBI_rgb_alpha);

        if (!m_Data)
        {
            LOGERROR("Failed to decode texture image: ", virtualPath);
            return false;
        }
        return true;
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

    const void* TextureLoader::GetData() const
    {
        return m_Data;
    }
}
