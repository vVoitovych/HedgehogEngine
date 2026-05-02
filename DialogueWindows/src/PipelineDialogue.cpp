#include "api/PipelineDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int plFilterNum = 1;
    char const* plFilterPatterns[plFilterNum] = { "*.pl" };
    const char* plFilterDescription = "Pipeline layout files (*.pl)";

    char* PipelineOpenDialogue()
    {
        return tinyfd_openFileDialog(
            "Open pipeline layout",
            "../",
            plFilterNum,
            plFilterPatterns,
            plFilterDescription,
            0);
    }

    char* PipelineSaveDialogue()
    {
        return tinyfd_saveFileDialog(
            "Save pipeline layout",
            "../",
            plFilterNum,
            plFilterPatterns,
            plFilterDescription);
    }
}
