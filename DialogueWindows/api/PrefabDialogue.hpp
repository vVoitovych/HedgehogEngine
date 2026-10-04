#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Where to save a new prefab, starting at defaultPath (a folder or a suggested file); null
    // when cancelled.
    DIALOGUE_WINDOWS_API char* PrefabSaveDialogue(const char* defaultPath);
}
