#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Pick a file to export a profiler capture to as Chrome trace JSON; nullptr when cancelled.
    // defaultPath is the folder (ending in a separator) or file the dialogue starts at.
    DIALOGUE_WINDOWS_API char* ProfilerTraceSaveDialogue(const char* defaultPath);
}
