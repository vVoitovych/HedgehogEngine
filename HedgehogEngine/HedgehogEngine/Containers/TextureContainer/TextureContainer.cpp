#include "TextureContainer.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void TextureContainer::RegisterTexturePath(const std::string& path)
    {
        auto it = std::find(m_TexturePathes.begin(), m_TexturePathes.end(), path);
        if (it == m_TexturePathes.end())
            m_TexturePathes.push_back(path);
    }

    const std::vector<std::string>& TextureContainer::GetTexturePathes() const
    {
        return m_TexturePathes;
    }

    size_t TextureContainer::GetTextureIndex(const std::string& name) const
    {
        auto it = std::find(m_TexturePathes.begin(), m_TexturePathes.end(), name);
        return static_cast<size_t>(it - m_TexturePathes.begin());
    }
}
