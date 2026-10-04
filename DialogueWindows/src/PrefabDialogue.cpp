#include "api/PrefabDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int prefabFilterNum = 1;
    char const* prefabFilterPatterns[prefabFilterNum] = { "*.prefab" };

    char* PrefabSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog(
            "Create prefab",
            defaultPath,
            prefabFilterNum,
            prefabFilterPatterns,
            "prefabs (*.prefab)");
    }
}
