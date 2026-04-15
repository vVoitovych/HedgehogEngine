#pragma once

#include "ContentLoaderApi.hpp"

#include <string>

namespace ContentLoader
{
    class TextureLoader
    {
    public:
        CONTENT_LOADER_API TextureLoader();
        CONTENT_LOADER_API ~TextureLoader();

        CONTENT_LOADER_API void LoadTexture(const std::string& file);

        CONTENT_LOADER_API int GetWidth()   const;
        CONTENT_LOADER_API int GetHeight()  const;
        CONTENT_LOADER_API int GetChanels() const;

        CONTENT_LOADER_API void* GetData() const;

    private:
        int   mWidth;
        int   mHeight;
        int   mChanels;
        void* mData;
    };
}
