#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // A project folder to open, starting at defaultPath; null when cancelled.
    DIALOGUE_WINDOWS_API char* ProjectOpenDialogue(const char* defaultPath);

    // Asks before the editor switches to the project named projectName, whose unsaved scene
    // changes are lost: true for OK.
    DIALOGUE_WINDOWS_API bool ConfirmProjectSwitch(const char* projectName);
}
