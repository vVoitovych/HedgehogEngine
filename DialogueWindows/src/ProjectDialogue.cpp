#include "api/ProjectDialogue.hpp"

#include "tinyfiledialogs/tinyfiledialogs.h"

#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShObjIdl.h>
#endif

namespace DialogueWindows
{
    namespace
    {
#ifdef _WIN32
        // The folder chooser as Explorer shows it (IFileOpenDialog picking folders). tinyfd's own
        // SHBrowseForFolder never shows on the editor's main thread, whose COM apartment is already
        // multithreaded (the audio engine initializes COM so), and the editor stops there for good.
        // The dialog runs on a thread of its own in a single-threaded apartment, with no owner
        // window: an owner on the waiting main thread would deadlock as the dialog disables it.
        std::optional<std::wstring> PickFolder(const std::wstring& title, const std::wstring& start)
        {
            std::optional<std::wstring> chosen;
            if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
                return chosen;

            IFileOpenDialog* dialog = nullptr;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&dialog))))
            {
                FILEOPENDIALOGOPTIONS options = 0;
                dialog->GetOptions(&options);
                dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
                if (!title.empty())
                    dialog->SetTitle(title.c_str());

                IShellItem* folder = nullptr;
                if (!start.empty() &&
                    SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&folder))))
                {
                    dialog->SetFolder(folder);
                    folder->Release();
                }

                IShellItem* result = nullptr;
                if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&result)))
                {
                    PWSTR path = nullptr;
                    if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                    {
                        chosen = path;
                        CoTaskMemFree(path);
                    }
                    result->Release();
                }
                dialog->Release();
            }
            CoUninitialize();
            return chosen;
        }

        std::filesystem::path Utf8Path(const char* text)
        {
            const std::string bytes(text != nullptr ? text : "");
            return std::filesystem::path(std::u8string(bytes.begin(), bytes.end()));
        }

        char* SelectFolder(const char* title, const char* defaultPath)
        {
            // UTF-8 in and out, as tinyfd's dialogs; the result lives until the next call.
            static std::string folder;
            const std::wstring titleText = Utf8Path(title).wstring();
            const std::wstring start =
                Utf8Path(defaultPath).make_preferred().wstring();

            std::optional<std::wstring> chosen;
            std::thread picker([&]() { chosen = PickFolder(titleText, start); });
            picker.join();
            if (!chosen)
                return nullptr;
            const std::u8string utf8 = std::filesystem::path(*chosen).u8string();
            folder.assign(utf8.begin(), utf8.end());
            return folder.data();
        }
#else
        char* SelectFolder(const char* title, const char* defaultPath)
        {
            return tinyfd_selectFolderDialog(title, defaultPath);
        }
#endif
    }

    char* ProjectOpenDialogue(const char* defaultPath)
    {
        return SelectFolder("Open project (a folder holding Project.yaml)", defaultPath);
    }

    char* ProjectFolderDialogue(const char* title, const char* defaultPath)
    {
        return SelectFolder(title, defaultPath);
    }

    bool ConfirmProjectSwitch(const char* projectName)
    {
        // No quotes: tinyfd refuses any text holding one (a project name never does).
        const std::string message = std::string("Open the project ") + projectName +
                                    "?\nThe editor restarts on it; unsaved changes to the scene are lost.";
        return tinyfd_messageBox("Open project", message.c_str(), "okcancel", "question", 0) == 1;
    }
}
