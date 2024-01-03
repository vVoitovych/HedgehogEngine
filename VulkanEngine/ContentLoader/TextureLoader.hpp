#pragma once

#include <string>


namespace ContentLoader
{
	class TextureLoader
	{
	public:
		TextureLoader();
		~TextureLoader();

		void LoadTexture(const std::string& file);

		int GetWidth() const;
		int GetHeight() const;
		int GetChanels() const;

		void* GetData() const;

	private:
		int mWidth;
		int mHeight;
		int mChanels;

		void* mData;
	};
}

