#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <string>
#include <vector>

namespace HedgehogEngine
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

        HEDGEHOG_ENGINE_API void RegisterTexturePath(const std::string& path);

        HEDGEHOG_ENGINE_API const std::vector<std::string>& GetTexturePathes() const;
        HEDGEHOG_ENGINE_API size_t                          GetTextureIndex(const std::string& name) const;

    private:
        std::vector<std::string> m_TexturePathes;
    };
}
