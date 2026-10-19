#include "api/AnimatorControllerDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

namespace DialogueWindows
{
    constexpr int animatorControllerFilterNum = 1;
    char const* animatorControllerFilterPatterns[animatorControllerFilterNum] = { "*.animctrl" };

    char* AnimatorControllerOpenDialogue(const char* defaultPath)
    {
        return tinyfd_openFileDialog(
            "Open animator controller",
            defaultPath,
            animatorControllerFilterNum,
            animatorControllerFilterPatterns,
            "animator controllers (*.animctrl)",
            0);
    }

    char* AnimatorControllerSaveDialogue(const char* defaultPath)
    {
        return tinyfd_saveFileDialog(
            "Save animator controller as",
            defaultPath,
            animatorControllerFilterNum,
            animatorControllerFilterPatterns,
            "animator controllers (*.animctrl)");
    }
}
