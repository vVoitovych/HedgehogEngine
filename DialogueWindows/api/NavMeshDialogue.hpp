#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Where to save a baked navmesh, starting at defaultPath (a folder or a suggested file); null
    // when cancelled.
    DIALOGUE_WINDOWS_API char* NavMeshSaveDialogue(const char* defaultPath);
}
