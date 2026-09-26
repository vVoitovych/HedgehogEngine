#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Pick a .graph file to open, or a file to save a graph as; nullptr when cancelled. defaultPath
    // is the folder (ending in a separator) or file the dialogue starts at.
    DIALOGUE_WINDOWS_API char* RenderGraphOpenDialogue(const char* defaultPath);
    DIALOGUE_WINDOWS_API char* RenderGraphSaveDialogue(const char* defaultPath);
}
