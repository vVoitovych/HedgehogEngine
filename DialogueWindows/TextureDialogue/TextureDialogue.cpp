#include "TextureDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int textureFilterNum = 2;
	char const* textureFilterPatterns[textureFilterNum] = { "*.png", "*.jpg" };
	const char* textureFilterDescription = "texture files (*.png, *.jpg)";

	char* TextureOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open texture",
			"../",
			textureFilterNum,
			textureFilterPatterns,
			textureFilterDescription,
			1);
	}

}