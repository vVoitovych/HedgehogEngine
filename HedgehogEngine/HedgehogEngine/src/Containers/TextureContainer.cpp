#include "HedgehogEngine/api/Containers/TextureContainer.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    void TextureContainer::RegisterTexturePath(const std::string& path)
    {
        std::string key = FS::MakeAssetKey(path);
        if (key.empty())
            return;
        auto it = std::find(m_TexturePathes.begin(), m_TexturePathes.end(), key);
        if (it == m_TexturePathes.end())
            m_TexturePathes.push_back(std::move(key));
    }

    const std::vector<std::string>& TextureContainer::GetTexturePathes() const
    {
        return m_TexturePathes;
    }

    size_t TextureContainer::GetTextureIndex(const std::string& name) const
    {
        auto it = std::find(m_TexturePathes.begin(), m_TexturePathes.end(), FS::MakeAssetKey(name));
        return static_cast<size_t>(it - m_TexturePathes.begin());
    }
}
