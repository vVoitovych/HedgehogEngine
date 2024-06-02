#include "TextureLoader.hpp"
#include "CommonFunctions.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "Libraries/stb/stb_image.hpp"

#include <stdexcept>

namespace ContentLoader
{

	TextureLoader::TextureLoader()
		: mData(nullptr)
		, mWidth(0)
		, mHeight(0)
		, mChanels(0)
	{
	}

	TextureLoader::~TextureLoader()
	{
		if (!mData)
		{
			stbi_image_free(mData);
		}
	}

	void TextureLoader::LoadTexture(const std::string& file)
	{
		std::string fullPath = GetAssetsDirectory() + file;
		mData = stbi_load(fullPath.c_str(), &mWidth, &mHeight, &mChanels, STBI_rgb_alpha);
		if (!mData)
		{
			throw std::runtime_error("failed to load texture image!");
		}
	}

	int TextureLoader::GetWidth() const
	{
		return mWidth;
	}

	int TextureLoader::GetHeight() const
	{
		return mHeight;
	}

	int TextureLoader::GetChanels() const
	{
		return mChanels;
	}

	void* TextureLoader::GetData() const
	{
		return mData;
	}

}

