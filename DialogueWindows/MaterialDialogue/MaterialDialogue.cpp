#include "MaterialDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int materialFilterNum = 1;
	char const* materialFilterPatterns[materialFilterNum] = { "*.material" };
	const char* materialFilterDescription = "material files (*.material)";

	constexpr int materialInstanceFilterNum = 1;
	char const* materialInstanceFilterPatterns[materialFilterNum] = { "*.minst" };
	const char* materialInstanceFilterDescription = "material instance files (*.minst)";

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
	char* MaterialInstanceCreationDialogue()
	{
		return tinyfd_saveFileDialog(
			"New material instance",
			"../",
			materialInstanceFilterNum,
			materialInstanceFilterPatterns,
			materialInstanceFilterDescription);
	}
	char* MaterialInstanceOpenDialogue()
	{
		return tinyfd_openFileDialog(
			"Open material instance",
			"../",
			materialInstanceFilterNum,
			materialInstanceFilterPatterns,
			materialInstanceFilterDescription,
			1);
	}
	char* MaterioalInstanceSaveDialogue()
	{
		return tinyfd_saveFileDialog(
			"Save material instance",
			"../",
			materialInstanceFilterNum,
			materialInstanceFilterPatterns,
			materialInstanceFilterDescription);
	}
}

