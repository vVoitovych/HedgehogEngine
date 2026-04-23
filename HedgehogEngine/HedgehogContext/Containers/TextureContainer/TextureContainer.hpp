#pragma once

#include <string>
#include <vector>

namespace Context
{
    class TextureContainer
    {
    public:
        TextureContainer()  = default;
        ~TextureContainer() = default;

        TextureContainer(const TextureContainer&)            = delete;
        TextureContainer(TextureContainer&&)                 = delete;
        TextureContainer& operator=(const TextureContainer&) = delete;
        TextureContainer& operator=(TextureContainer&&)      = delete;

        void RegisterTexturePath(const std::string& path);

        const std::vector<std::string>& GetTexturePathes() const;
        size_t                          GetTextureIndex(const std::string& name) const;

    private:
        std::vector<std::string> m_TexturePathes;
    };
}
