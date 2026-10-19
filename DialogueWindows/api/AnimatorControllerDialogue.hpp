#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Pick an .animctrl file to open, or a file to save an animator controller as; nullptr when
    // cancelled. defaultPath is the folder (ending in a separator) or file the dialogue starts at.
    DIALOGUE_WINDOWS_API char* AnimatorControllerOpenDialogue(const char* defaultPath);
    DIALOGUE_WINDOWS_API char* AnimatorControllerSaveDialogue(const char* defaultPath);
}
