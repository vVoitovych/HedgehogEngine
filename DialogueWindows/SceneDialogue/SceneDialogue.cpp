#include "SceneDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	const int sceneFilterNum = 1;
	char const* sceneFilterPatterns[sceneFilterNum] = { "*.yaml" };

	char* SceneOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open scene",
			"../",
			sceneFilterNum,
			sceneFilterPatterns,
			"scene files (*.yaml)",
			1);
	}

	char* SceneSaveDialogue()
	{
		return tinyfd_saveFileDialog(
			"Save scene",
			"../",
			sceneFilterNum,
			sceneFilterPatterns,
			"scene files (*.yaml)");
	}

	char* SceneRenameDialogue()
	{
		return tinyfd_inputBox("Rename", "Test message", "New Scene");
	}

}

