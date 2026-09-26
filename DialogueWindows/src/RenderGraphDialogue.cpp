#include "api/RenderGraphDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int renderGraphFilterNum = 1;
    char const* renderGraphFilterPatterns[renderGraphFilterNum] = { "*.graph" };

    char* RenderGraphOpenDialogue(const char* defaultPath)
    {
        return tinyfd_openFileDialog(
            "Open render graph",
            defaultPath,
            renderGraphFilterNum,
            renderGraphFilterPatterns,
            "render graphs (*.graph)",
            0);
    }

    char* RenderGraphSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog(
            "Save render graph as",
            defaultPath,
            renderGraphFilterNum,
            renderGraphFilterPatterns,
            "render graphs (*.graph)");
    }
}
