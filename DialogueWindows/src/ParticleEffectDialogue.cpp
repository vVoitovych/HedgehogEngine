#include "api/ParticleEffectDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int particleEffectFilterNum = 1;
    char const* particleEffectFilterPatterns[particleEffectFilterNum] = { "*.particle" };

    char* ParticleEffectOpenDialogue(const char* defaultPath)
    {
        return tinyfd_openFileDialog("Open particle effect", defaultPath, particleEffectFilterNum,
                                     particleEffectFilterPatterns, "particle effects (*.particle)", 0);
    }

    char* ParticleEffectSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog("Save particle effect as", defaultPath, particleEffectFilterNum,
                                     particleEffectFilterPatterns, "particle effects (*.particle)");
    }
}
