#pragma once

#include "HedgehogInput/api/InputActionMap.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

// The project's actions file on disk: loading it and noticing when it is saved again.
namespace HInput
{
    // How often PollInputActions looks at the file.
    inline constexpr std::chrono::seconds INPUT_ACTIONS_POLL_INTERVAL{ 1 };

    // Reads and parses the actions file at virtualPath. A file that cannot be read or does not parse
    // logs "[Input] <path>: <error>" and gives nullopt.
    [[nodiscard]] std::optional<InputActionSet> LoadInputActions(const std::string&           virtualPath,
                                                                 const FS::FileSystemManager& fileSystem);

    // What PollInputActions remembers between polls: plain data.
    struct InputActionsWatch
    {
        std::string                                          VirtualPath;
        std::optional<std::filesystem::file_time_type>       WriteTime;       // of the last version read
        std::optional<std::filesystem::file_time_type>       FailedWriteTime; // a version already reported broken
        std::optional<std::chrono::steady_clock::time_point> LastPoll;
    };

    // A watch on virtualPath, taking the file's current version as already read, so the first poll
    // reloads nothing.
    [[nodiscard]] InputActionsWatch WatchInputActions(const std::string& virtualPath, const FS::FileSystemManager& fileSystem);

    // At most once per INPUT_ACTIONS_POLL_INTERVAL (now is a parameter, so tests need not wait):
    // when the file's write time has moved on, reads it into out and logs "[Input] Reloaded <path>.",
    // returning true. A version that does not parse leaves out as it was and logs its error once; the
    // next save is read again. A missing file changes nothing.
    bool PollInputActions(InputActionsWatch& watch, const FS::FileSystemManager& fileSystem,
                          std::chrono::steady_clock::time_point now, InputActionSet& out);
}
