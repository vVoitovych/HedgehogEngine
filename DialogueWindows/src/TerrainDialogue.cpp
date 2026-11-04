#include "api/TerrainDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int terrainFilterNum = 1;
    char const* terrainFilterPatterns[terrainFilterNum] = { "*.terrain" };

    char* TerrainSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog(
            "Create terrain",
            defaultPath,
            terrainFilterNum,
            terrainFilterPatterns,
            "terrains (*.terrain)");
    }
}
