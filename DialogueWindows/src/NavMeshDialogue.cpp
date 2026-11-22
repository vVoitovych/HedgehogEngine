#include "api/NavMeshDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    char* NavMeshSaveDialogue(const char* defaultPath)
    {
        constexpr int filterCount          = 1;
        const char*   filters[filterCount] = { "*.navmesh" };
        return tinyfd_saveFileDialog("Bake navmesh", defaultPath, filterCount, filters, "navmeshes (*.navmesh)");
    }
}
