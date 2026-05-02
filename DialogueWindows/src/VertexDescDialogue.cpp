#include "api/VertexDescDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int vdesFilterNum = 1;
    char const* vdesFilterPatterns[vdesFilterNum] = { "*.vdes" };
    const char* vdesFilterDescription = "Vertex description files (*.vdes)";

    char* VertexDescOpenDialogue()
    {
        return tinyfd_openFileDialog(
            "Open vertex description",
            "../",
            vdesFilterNum,
            vdesFilterPatterns,
            vdesFilterDescription,
            0);
    }

    char* VertexDescSaveDialogue()
    {
        return tinyfd_saveFileDialog(
            "Save vertex description",
            "../",
            vdesFilterNum,
            vdesFilterPatterns,
            vdesFilterDescription);
    }
}
