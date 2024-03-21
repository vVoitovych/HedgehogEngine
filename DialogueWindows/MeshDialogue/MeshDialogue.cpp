#include "MeshDialogue.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int meshFilterNum = 1;
	char const* meshFilterPatterns[meshFilterNum] = { "*.obj" };

	char* MeshOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open mesh",
			"../",
			meshFilterNum,
			meshFilterPatterns,
			"mesh files (*.obj)",
			1);
	}
}




