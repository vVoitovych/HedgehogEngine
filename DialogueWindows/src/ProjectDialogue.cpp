#include "api/ProjectDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

#include <string>

namespace DialogueWindows
{
    char* ProjectOpenDialogue(const char* defaultPath)
    {
        return tinyfd_selectFolderDialog("Open project (a folder holding Project.yaml)", defaultPath);
    }

    bool ConfirmProjectSwitch(const char* projectName)
    {
        const std::string message = std::string("Open the project '") + projectName +
                                    "'?\nThe editor restarts on it; unsaved changes to the scene are lost.";
        return tinyfd_messageBox("Open project", message.c_str(), "okcancel", "question", 0) == 1;
    }
}
