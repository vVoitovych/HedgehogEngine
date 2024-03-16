#include "SceneDialogue.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	const int filterNum = 1;
	char const* lFilterPatterns[filterNum] = { "*.yaml" };

	char* SceneOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open scene",
			"../",
			filterNum,
			lFilterPatterns,
			"scene files (*.yaml)",
			1);
	}

	char* SceneSaveDialogue()
	{
		return tinyfd_saveFileDialog(
			"Save scene",
			"../",
			filterNum,
			lFilterPatterns,
			"scene files (*.yaml)");
	}

	char* SceneRenameDialogue()
	{
		return tinyfd_inputBox("Rename", "Test message", "New Scene");
	}

}

