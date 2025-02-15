#include "MeshDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int meshFilterNum = 2;
	char const* meshFilterPatterns[meshFilterNum] = { "*.obj", "*.gltf"};

	char* MeshOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open mesh",
			"../",
			meshFilterNum,
			meshFilterPatterns,
			"mesh files (*.obj, *.gltf)",
			1);
	}
}




