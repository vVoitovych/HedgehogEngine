#include "MaterialDialogue.hpp"

#include "ThirdParty/tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int filterNum = 1;
	char const* lFilterPatterns[filterNum] = { "*.mat" };

	char* MaterialCreationDialogue()
	{
		return tinyfd_saveFileDialog(
			"New material",
			"../",
			filterNum,
			lFilterPatterns,
			"material files (*.mat)");
	}
	char* MaterialOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open material",
			"../",
			filterNum,
			lFilterPatterns,
			"material files (*.mat)",
			1);
	}

	char* MaterioalSaveDialogue()
	{
		return tinyfd_saveFileDialog(
			"Save material",
			"../",
			filterNum,
			lFilterPatterns,
			"material files (*.mat)");
	}
}