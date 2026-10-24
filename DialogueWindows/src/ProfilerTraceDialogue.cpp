#include "api/ProfilerTraceDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int profilerTraceFilterNum = 1;
    char const* profilerTraceFilterPatterns[profilerTraceFilterNum] = { "*.json" };

    char* ProfilerTraceSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog(
            "Export profiler capture",
            defaultPath,
            profilerTraceFilterNum,
            profilerTraceFilterPatterns,
            "Chrome trace files (*.json)");
    }
}
