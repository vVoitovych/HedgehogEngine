#include "SceneOpenDialogue.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	char* SceneOpenDialogue()
	{
		char const* lFilterPatterns[2] = { "*.yaml" };
		return tinyfd_openFileDialog(
			"Open scene",
			"../",
			1,
			lFilterPatterns,
			"scene files",
			1);
	}

}

