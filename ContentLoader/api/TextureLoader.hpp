#pragma once

#include "ContentLoaderApi.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <string>

namespace ContentLoader
{
    class TextureLoader
    {
    public:
        CONTENT_LOADER_API TextureLoader();
        CONTENT_LOADER_API ~TextureLoader();

        TextureLoader(const TextureLoader&)            = delete;
        TextureLoader& operator=(const TextureLoader&) = delete;
        TextureLoader(TextureLoader&&)                 = delete;
        TextureLoader& operator=(TextureLoader&&)      = delete;

        // Returns false (after logging) when the file cannot be read or decoded;
        // GetData() stays null in that case.
        CONTENT_LOADER_API bool LoadTexture(const std::string& file,
                                             const FS::FileSystemManager& fileSystem);

        CONTENT_LOADER_API int GetWidth()   const;
        CONTENT_LOADER_API int GetHeight()  const;
        CONTENT_LOADER_API int GetChannels() const;

        CONTENT_LOADER_API const void* GetData() const;

    private:
        int   m_Width;
        int   m_Height;
        int   m_Channels;
        void* m_Data;
    };
}
