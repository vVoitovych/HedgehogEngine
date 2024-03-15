#include "SceneDialogue.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	char* SceneOpenDialogue()
	{
		char const* lFilterPatterns[1] = { "*.yaml" };
		return tinyfd_openFileDialog(
			"Open scene",
			"../",
			1,
			lFilterPatterns,
			"scene files",
			1);
	}

	char* SceneSaveDialogue()
	{
		char const* lFilterPatterns[1] = { "*.yaml" };
		return tinyfd_saveFileDialog(
			"Save scene",
			"../",
			1,
			lFilterPatterns,
			"scene files");
	}

	char* SceneRenameDialogue()
	{
		return tinyfd_inputBox("Rename", "Test message", "New Scene");
	}

}

