#include "ScriptDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
	constexpr int scriptFilterNum = 1;
	char const* scriptFilterPatterns[scriptFilterNum] = { "*.lua" };
	const char* scriptFilterDescription = "script files (*.lua)";

	char* ScriptChooseDialogue()
	{
		return tinyfd_openFileDialog(
			"Choose script",
			"../",
			scriptFilterNum,
			scriptFilterPatterns,
			scriptFilterDescription,
			1);
	}
}

