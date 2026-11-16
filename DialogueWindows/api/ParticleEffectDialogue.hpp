#pragma once

#include "DialogueWindowsAPI.hpp"

namespace DialogueWindows
{
    // Pick a .particle file to open, or a file to save a particle effect as; nullptr when
    // cancelled. defaultPath is the folder (ending in a separator) or file the dialogue starts at.
    DIALOGUE_WINDOWS_API char* ParticleEffectOpenDialogue(const char* defaultPath);
    DIALOGUE_WINDOWS_API char* ParticleEffectSaveDialogue(const char* defaultPath);
}
