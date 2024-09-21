#include "MaterialDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int materialFilterNum = 1;
	char const* materialFilterPatterns[materialFilterNum] = { "*.material" };
	const char* materialFilterDescription = "material files (*.material)";

	char* MaterialCreationDialogue()
	{
		return tinyfd_saveFileDialog(
			"New material",
			"../",
			materialFilterNum,
			materialFilterPatterns,
			materialFilterDescription);
	}
	char* MaterialOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open material",
			"../",
			materialFilterNum,
			materialFilterPatterns,
			materialFilterDescription,
			1);
	}

	char* MaterioalSaveDialogue()
	{
		return tinyfd_saveFileDialog(
			"Save material",
			"../",
			materialFilterNum,
			materialFilterPatterns,
			materialFilterDescription);
	}
}

